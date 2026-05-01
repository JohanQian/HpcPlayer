# SurfaceTexture + OES Texture + EGL 渲染方案

## 方案概述

本方案在 JNI 和 C++ 层完整实现了 SurfaceTexture + OES 纹理 + EGL 渲染的高性能媒体播放器架构。所有的 OpenGL ES 和 EGL 操作都在专用的后台渲染线程中进行，与 UI 线程完全隔离。

## 架构设计

### 1. 核心层次结构

```
Java UI 层 (MainActivity + GLTextureView)
    ↓ JNI 接口层 (hpcplayer-native.cpp)
    ↓ 
C++ 核心层 (HpcPlayer)
    ↓
C++ 渲染层 (SurfaceTextureRenderer)
    ├─ EGL 管理 (EglEnv)
    ├─ OpenGL ES 着色器 (GLSL)
    └─ SurfaceTexture + OES 纹理处理
```

### 2. 关键组件

#### Java 层

**GLTextureView.java**
- 继承 TextureView 实现自定义纹理视图
- 实现 TextureView.SurfaceTextureListener 接口
- 当 SurfaceTexture 可用时，将其传递给 C++ 层
- 负责生命周期管理（创建、销毁、大小改变等事件）

**MainActivity.java**
- 实现 GLTextureView.OnSurfaceTextureCreatedListener 接口
- 管理 UI 控制和用户交互
- 与 HpcMediaPlayer 交互进行播放控制

**HpcMediaPlayer.java**
- 新增 `setSurfaceTexture(Object surfaceTexture)` 方法
- 通过 JNI 调用 `nativeSetSurfaceTexture()` 将 SurfaceTexture 传递给 C++

#### JNI 层 (hpcplayer-native.cpp)

```cpp
JNIEXPORT jint JNICALL
Java_com_example_hpcplayer_HpcMediaPlayer_nativeSetSurfaceTexture(
    JNIEnv* env, jobject thiz, jlong player_ptr, jobject surfaceTextureObj)
```

功能：
- 创建 OES 纹理（通过 GLUtils::createOesTexture()）
- 创建 SurfaceTexture 和 Surface 对象
- 获取 ANativeWindow
- 将 ANativeWindow 传递给 HpcPlayer

#### C++ 层 (SurfaceTextureRenderer)

**核心职责：**
1. **EGL 环境管理** (EglEnv)
   - EGL Display 初始化
   - EGL Context 创建
   - EGL Surface 创建

2. **OpenGL ES 渲染**
   - OES 纹理着色器程序
   - 顶点缓冲区和纹理坐标缓冲区
   - 矩阵变换

3. **渲染线程管理**
   - 专用的 std::thread 进行渲染
   - 消息队列驱动的渲染循环
   - 线程安全的同步机制

### 3. 数据流

```
用户操作（播放/暂停）
    ↓
MainActivity 调用 HpcMediaPlayer 方法
    ↓
HpcMediaPlayer 通过 JNI 调用 C++ 接口
    ↓
HpcPlayer::start()/pause()/stop()
    ↓
发送消息到 SurfaceTextureRenderer
    ↓
渲染线程处理消息
    ↓
通过 SurfaceTexture 获取最新视频帧
    ↓
绑定 OES 纹理并渲染
    ↓
通过 EGL 交换缓冲区显示
```

### 4. 渲染流程

```
renderThreadMain() {
    1. 初始化 EglEnv
    2. 编译着色器程序（OES 纹理采样）
    3. 设置顶点缓冲区和纹理坐标
    4. 进入主循环：
        - 等待帧数据准备好
        - MakeCurrent 绑定 EGL Context
        - 绑定 OES 纹理
        - 执行绘制调用
        - SwapBuffers 交换缓冲区
}
```

## 技术细节

### OES 纹理

OES_EXT_IMAGE_EXTERNAL 是 Android 专有的 OpenGL 扩展，用于处理外部纹理数据：

**顶点着色器：**
```glsl
attribute vec4 a_position;
attribute vec2 a_texCoord;
uniform mat4 u_matrix;
varying vec2 v_texCoord;

void main() {
    gl_Position = u_matrix * a_position;
    v_texCoord = a_texCoord;
}
```

**片段着色器：**
```glsl
#extension GL_OES_EGL_image_external : require
precision mediump float;
uniform samplerExternalOES u_texture;
varying vec2 v_texCoord;

void main() {
    gl_FragColor = texture2D(u_texture, v_texCoord);
}
```

关键点：
- `samplerExternalOES` 是 OES 纹理采样器
- `GL_TEXTURE_EXTERNAL_OES` 用于绑定纹理
- 必须申明扩展：`#extension GL_OES_EGL_image_external : require`

### EGL 上下文绑定

```cpp
// 在渲染线程中
eglMakeCurrent(display, surface, surface, context);
// 执行 OpenGL 操作
eglSwapBuffers(display, surface);
```

注意：EGL 上下文必须在创建线程中使用，不能跨线程。

### 线程同步

使用 C++ 的标准库工具：
```cpp
std::mutex mRenderLock;
std::condition_variable mRenderCond;

// 主线程通知
mFramePrepared = true;
mRenderCond.notify_one();

// 渲染线程等待
std::unique_lock<std::mutex> lock(mRenderLock);
mRenderCond.wait(lock, [this] { return mFramePrepared || !mRenderThreadRunning; });
```

## 编译配置

### CMakeLists.txt 需要调整

确保包含以下库：
```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(hpcplayer-native 
    ${OPENGL_LIBRARIES}
    EGL
    GLESv2
    android
    log
)
```

### build.gradle 配置

```gradle
android {
    defaultConfig {
        minSdkVersion 21  // OpenGL ES 2.0 + EGL 支持
    }
    
    externalNativeBuild {
        cmake {
            path "src/main/cpp/CMakeLists.txt"
        }
    }
}
```

## 性能特点

### 优势

1. **硬件加速** - 完全利用 GPU，视频帧直接送入 OES 纹理
2. **低延迟** - 专用渲染线程，不阻塞 UI 线程
3. **灵活变换** - 支持矩阵变换（旋转、缩放、平移）
4. **良好扩展** - 易于添加滤镜、后处理等效果
5. **兼容性强** - OES 纹理在 Android 4.0+ 设备上广泛支持

### 资源占用

- 内存：OES 纹理相对较小（取决于视频分辨率）
- CPU：主线程几乎无负担（仅处理消息和 UI）
- GPU：专用于视频渲染

## 扩展点

### 1. 添加视频滤镜

修改 SurfaceTextureRenderer 中的片段着色器：
```glsl
void main() {
    vec4 color = texture2D(u_texture, v_texCoord);
    // 应用滤镜效果
    gl_FragColor = applyFilter(color);
}
```

### 2. 添加字幕渲染

在 drawFrame() 中追加额外的绘制调用，使用不同的纹理和着色器。

### 3. 添加视频录制

捕获 EGL 缓冲区内容，使用 MediaCodec 编码成文件。

### 4. HDR 支持

- 使用 GL_RGB10_A2 或 GL_RGBA16F 等高精度纹理格式
- 在着色器中进行色彩空间转换

## 调试建议

### 启用日志

在 C++ 代码中使用：
```cpp
#include <android/log.h>
#define LOG_TAG "SurfaceTextureRenderer"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
```

### 检查 OpenGL 错误

```cpp
GLenum error = glGetError();
if (error != GL_NO_ERROR) {
    LOG_E("OpenGL error: 0x%x", error);
}
```

### 检查 EGL 错误

```cpp
EGLint error = eglGetError();
if (error != EGL_SUCCESS) {
    LOG_E("EGL error: 0x%x", error);
}
```

## 集成步骤总结

1. **编译** C++ 代码（SurfaceTextureRenderer、GLUtils 等）
2. **更新** CMakeLists.txt 确保包含所有源文件
3. **运行** gradle build 编译项目
4. **安装** APK 到设备
5. **测试** 播放功能和渲染效果

## 已实现的文件

- `SurfaceTextureRenderer.h/cpp` - 核心渲染器
- `GLTextureView.java` - 自定义 TextureView
- `hpcplayer-native.cpp` - 新增 JNI 方法
- `HpcMediaPlayer.java` - 新增 Java 接口方法
- `activity_video_player.xml` - 布局文件更新
- `MainActivity.java` - UI 层集成

## 注意事项

1. **EGL 上下文只能在创建线程中使用** - SurfaceTextureRenderer 在专用线程中管理所有 EGL 操作
2. **SurfaceTexture 线程安全** - 通过消息队列异步处理所有操作
3. **纹理绑定生命周期** - OES 纹理必须在 EGL Context 当前时创建和销毁
4. **权限** - 需要 READ_EXTERNAL_STORAGE 或 READ_MEDIA_VIDEO 权限
5. **设备兼容性** - 要求 Android 4.0+ (API 14+)，OpenGL ES 2.0+


