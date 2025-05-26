//
// Created by Administrator on 2025/4/12.
//

#include "MetaData.h"

namespace hpc {

MetaData::MetaData(const MetaData &from) {

}

MetaData& MetaData::operator = (const MetaData &rhs) {
  this->height = rhs.height;
  return *this;
}
} // hpc