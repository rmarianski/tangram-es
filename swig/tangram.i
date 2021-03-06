// swig -v -c++  -I../core/src -o ../android/tangram/jni/jniGenerated.cpp -outdir ../android/tangram/src/com/mapzen/tangram/ -package com.mapzen.tangram -java tangram.i
// checking internals :)
// swig -v -c++ -E -java tangram.i
// swig -c++ -java -debug-module 4 tangram.i

%module Tangram

%{
#include "tangram.h"
#include "data/properties.h"
#include "util/types.h"
#include <string>
#include <memory>
%}

%include "typemaps.i"
%include "std_common.i"
%include "std_string.i"
%include "std_vector.i"
%include "std_shared_ptr.i"

// Uppercase all enum items
%rename("%(uppercase)s", %$isenumitem) "";

// http://www.swig.org/Doc3.0/Java.html#Java_imclass_pragmas
// Change class modifier of the native methods intermediate class (i.e. TangramJNI)
// %pragma(java) jniclassclassmodifiers="class"
// Change class modifier of the module class (i.e. Tangram)
// %pragma(java) moduleclassmodifiers="class"

%shared_ptr(Tangram::Properties);

namespace Tangram {
struct Properties {

    Properties() {}

    void clear() { props.clear(); }

    bool contains(const std::string& key) const;

    double getNumber(const std::string& key) const;

    const std::string& getString(const std::string& key) const;
};
}
%extend Tangram::Properties {
    void set(std::string key, std::string value) {
        $self->set(key, value);
    }
    void set(std::string key, double value) {
        $self->set(key, value);
    }
}

// Include external description for
// - LngLat
// - Coordinates aka std::vector<LngLat>
// - Polygon aka std::vector<Coordinates>
%include "jni_geometry.i"

// Include external description for
// - Tags aka map<string,string>
// - DataSource
// - ClientGeoJsonSource
%include "jni_datasource.i"
%include "jni_debug.i"

namespace Tangram {
void addDataSource(std::shared_ptr<Tangram::DataSource> _source);
void clearDataSource(DataSource& _source, bool _data, bool _tiles);
void setDebugFlag(DebugFlags _flag, bool _on);
}
