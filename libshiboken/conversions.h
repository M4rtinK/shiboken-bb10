/*
 * This file is part of the Shiboken Python Bindings Generator project.
 *
 * Copyright (C) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: PySide team <contact@pyside.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include "sbkpython.h"
#include <limits>
#include <memory>
#include <typeinfo>

#include "sbkstring.h"
#include "sbkenum.h"
#include "basewrapper.h"
#include "bindingmanager.h"
#include "sbkdbg.h"

// When the user adds a function with an argument unknown for the typesystem, the generator writes type checks as
// TYPENAME_Check, so this macro allows users to add PyObject arguments to their added functions.
#define PyObject_Check(X) true
#define SbkChar_Check(X) (SbkNumber_Check(X) || Shiboken::String::checkChar(X))
#include "autodecref.h"

namespace Shiboken
{
/**
*   This function template is used to get the PyTypeObject of a C++ type T.
*   All implementations should be provided by template specializations generated by the generator when
*   T isn't a C++ primitive type.
*   \see SpecialCastFunction
*/
template<typename T>
PyTypeObject* SbkType()
{
    return 0;
}

template<> inline PyTypeObject* SbkType<int>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<unsigned int>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<short>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<unsigned short>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<long>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<unsigned long>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<PY_LONG_LONG>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<unsigned PY_LONG_LONG>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<bool>() { return &PyBool_Type; }
template<> inline PyTypeObject* SbkType<float>() { return &PyFloat_Type; }
template<> inline PyTypeObject* SbkType<double>() { return &PyFloat_Type; }
template<> inline PyTypeObject* SbkType<char>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<signed char>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<unsigned char>() { return &PyInt_Type; }

/**
 * Convenience template to create wrappers using the proper Python type for a given C++ class instance.
 */
template<typename T>
inline PyObject* createWrapper(const T* cppobj, bool hasOwnership = false, bool isExactType = false)
{
    const char* typeName = 0;
    if (!isExactType)
        typeName = typeid(*const_cast<T*>(cppobj)).name();
    return Object::newObject(reinterpret_cast<SbkObjectType*>(SbkType<T>()),
                              const_cast<T*>(cppobj), hasOwnership, isExactType, typeName);
}

// Base Conversions ----------------------------------------------------------
// The basic converter must be empty to avoid object types being converted by value.
template <typename T> struct Converter {};

// Pointer conversion specialization for value types.
template <typename T>
struct Converter<T*>
{
    static inline bool checkType(PyObject* pyObj)
    {
        return Converter<T>::checkType(pyObj);
    }

    static inline bool isConvertible(PyObject* pyObj)
    {
        return pyObj == Py_None || PyObject_TypeCheck(pyObj, SbkType<T>());
    }

    static PyObject* toPython(const T* cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        PyObject* pyobj = reinterpret_cast<PyObject*>(BindingManager::instance().retrieveWrapper(cppobj));
        if (pyobj)
            Py_INCREF(pyobj);
        else
            pyobj = createWrapper<T>(cppobj);
        return pyobj;
    }

    static T* toCpp(PyObject* pyobj)
    {
        if (PyObject_TypeCheck(pyobj, SbkType<T>()))
            return (T*) Object::cppPointer(reinterpret_cast<SbkObject*>(pyobj), SbkType<T>());
        else if (Converter<T>::isConvertible(pyobj))
            return new T(Converter<T>::toCpp(pyobj));
        else if (pyobj == Py_None)
            return 0;

        assert(false);
        return 0;
    }
};
template <typename T> struct Converter<const T*> : Converter<T*> {};

// Specialization for reference conversions.
template <typename T>
struct Converter<T&>
{
    static inline bool checkType(PyObject* pyObj) { return Converter<T>::checkType(pyObj); }
    static inline bool isConvertible(PyObject* pyObj) { return Converter<T>::isConvertible(pyObj); }
    static inline PyObject* toPython(const T& cppobj) { return Converter<T*>::toPython(&cppobj); }
    static inline T& toCpp(PyObject* pyobj) { return *Converter<T*>::toCpp(pyobj); }
};

// Void pointer conversions.
template<>
struct Converter<void*>
{
    static inline bool checkType(PyObject* pyObj) { return false; }
    static inline bool isConvertible(PyObject* pyobj) { return true; }
    static PyObject* toPython(void* cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        PyObject* result = (PyObject*) cppobj;
        Py_INCREF(result);
        return result;
    }
    static void* toCpp(PyObject* pyobj) { return pyobj; }
};

// Base converter meant to be inherited by converters for classes that could be
// passed by value.
// Example: "struct Converter<ValueTypeClass> : ValueTypeConverter<ValueTypeClass>"
template <typename T>
struct ValueTypeConverter
{
    static inline bool checkType(PyObject* pyObj) { return PyObject_TypeCheck(pyObj, SbkType<T>()); }

    // The basic version of this method also tries to use the extended 'isConvertible' method.
    static inline bool isConvertible(PyObject* pyobj)
    {
        if (PyObject_TypeCheck(pyobj, SbkType<T>()))
            return true;
        SbkObjectType* shiboType = reinterpret_cast<SbkObjectType*>(SbkType<T>());
        return ObjectType::isExternalConvertible(shiboType, pyobj);
    }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<T*>(cppobj)); }
    static inline PyObject* toPython(const T& cppobj)
    {
        PyObject* obj = createWrapper<T>(new T(cppobj), true, true);
//         SbkBaseWrapper_setContainsCppWrapper(obj, SbkTypeInfo<T>::isCppWrapper);
        return obj;
    }
    // Classes with implicit conversions are expected to reimplement 'toCpp' to build T from
    // its various implicit constructors. Even classes without implicit conversions could
    // get some of those via other modules defining conversion operator for them, thus
    // the basic Converter for value types checks for extended conversion and tries to
    // use them if it is the case.
    static inline T toCpp(PyObject* pyobj)
    {
        if (!PyObject_TypeCheck(pyobj, SbkType<T>())) {
            SbkObjectType* shiboType = reinterpret_cast<SbkObjectType*>(SbkType<T>());
            if (ObjectType::hasExternalCppConversions(shiboType) && isConvertible(pyobj)) {
                T* cptr = reinterpret_cast<T*>(ObjectType::callExternalCppConversion(shiboType, pyobj));
                std::auto_ptr<T> cptr_auto_ptr(cptr);
                return *cptr;
            }
            assert(false);
        }
        return *reinterpret_cast<T*>(Object::cppPointer(reinterpret_cast<SbkObject*>(pyobj), SbkType<T>()));
    }
};

// Base converter meant to be inherited by converters for abstract classes and object types
// (i.e. classes with private copy constructors and = operators).
// Example: "struct Converter<AbstractClass*> : ObjectTypeConverter<AbstractClass>"
template <typename T>
struct ObjectTypeConverter
{
    static inline bool checkType(PyObject* pyObj) { return pyObj == Py_None || PyObject_TypeCheck(pyObj, SbkType<T>()); }
    /// Py_None objects are the only objects convertible to an object type (in the form of a NULL pointer).
    static inline bool isConvertible(PyObject* pyObj) { return pyObj == Py_None || PyObject_TypeCheck(pyObj, SbkType<T>()); }
    /// Convenience overload that calls "toPython(const T*)" method.
    static inline PyObject* toPython(void* cppobj) { return toPython(reinterpret_cast<T*>(cppobj)); }
    /// Returns a new Python wrapper for the C++ object or an existing one with its reference counter incremented.
    static PyObject* toPython(const T* cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        PyObject* pyobj = reinterpret_cast<PyObject*>(BindingManager::instance().retrieveWrapper(cppobj));
        if (pyobj)
            Py_INCREF(pyobj);
        else
            pyobj = createWrapper<T>(cppobj);
        return pyobj;
    }
    /// Returns the wrapped C++ pointer casted properly, or a NULL pointer if the argument is a Py_None.
    static T* toCpp(PyObject* pyobj)
    {
        if (pyobj == Py_None)
            return 0;
        SbkObjectType* shiboType = reinterpret_cast<SbkObjectType*>(pyobj->ob_type);
        if (ObjectType::hasCast(shiboType))
            return reinterpret_cast<T*>(ObjectType::cast(shiboType, reinterpret_cast<SbkObject*>(pyobj), SbkType<T>()));
        return (T*) Object::cppPointer(reinterpret_cast<SbkObject*>(pyobj), SbkType<T>());
    }
};

template <typename T>
struct ObjectTypeReferenceConverter : ObjectTypeConverter<T>
{
    static inline bool checkType(PyObject* pyObj) { return PyObject_TypeCheck(pyObj, SbkType<T>()); }
    static inline bool isConvertible(PyObject* pyObj) { return PyObject_TypeCheck(pyObj, SbkType<T>()); }
    static inline PyObject* toPython(const T& cppobj) { return Converter<T*>::toPython(&cppobj); }
    static inline T& toCpp(PyObject* pyobj)
    {
        T* t = Converter<T*>::toCpp(pyobj);
        assert(t);
        return *t;
    }
};

// PyObject* specialization to avoid converting what doesn't need to be converted.
template<>
struct Converter<PyObject*> : ObjectTypeConverter<PyObject*>
{
    static inline PyObject* toCpp(PyObject* pyobj) { return pyobj; }
};

// Primitive Conversions ------------------------------------------------------
template <>
struct Converter<bool>
{
    static inline bool checkType(PyObject* pyobj) { return PyBool_Check(pyobj); }
    static inline bool isConvertible(PyObject* pyobj) { return PyInt_Check(pyobj); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<bool*>(cppobj)); }
    static inline PyObject* toPython(bool cppobj) { return PyBool_FromLong(cppobj); }
    static inline bool toCpp(PyObject* pyobj) { return PyInt_AS_LONG(pyobj); }
};

/**
 * Helper template for checking if a value overflows when casted to type T
 */
template<typename T, bool isSigned = std::numeric_limits<T>::is_signed >
struct OverFlowChecker;

template<typename T>
struct OverFlowChecker<T, true>
{
    static bool check(const PY_LONG_LONG& value)
    {
        return value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max();
    }
};

template<typename T>
struct OverFlowChecker<T, false>
{
    static bool check(const PY_LONG_LONG& value)
    {
        return value < 0 || static_cast<unsigned long long>(value) > std::numeric_limits<T>::max();
    }
};

template<>
struct OverFlowChecker<PY_LONG_LONG, true>
{
    static bool check(const PY_LONG_LONG& value)
    {
        return false;
    }
};

template<>
struct OverFlowChecker<double, true>
{
    static bool check(const double& value)
    {
        return false;
    }
};

template<>
struct OverFlowChecker<float, true>
{
    static bool check(const double& value)
    {
        return value < std::numeric_limits<float>::min() || value > std::numeric_limits<float>::max();
    }
};

template <typename PyIntEquiv>
struct Converter_PyInt
{
    static inline bool checkType(PyObject* pyobj) { return PyInt_Check(pyobj); }
    static inline bool isConvertible(PyObject* pyobj) { return SbkNumber_Check(pyobj); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<PyIntEquiv*>(cppobj)); }
    static inline PyObject* toPython(const PyIntEquiv& cppobj) { return PyInt_FromLong((long) cppobj); }
    static PyIntEquiv toCpp(PyObject* pyobj)
    {
        if (PyFloat_Check(pyobj)) {
            double d_result = PyFloat_AS_DOUBLE(pyobj);
            // If cast to long directly it could overflow silently
            if (OverFlowChecker<PyIntEquiv>::check(d_result))
                PyErr_SetObject(PyExc_OverflowError, 0);
            return static_cast<PyIntEquiv>(d_result);
        } else {
            PY_LONG_LONG result = PyLong_AsLongLong(pyobj);
            if (OverFlowChecker<PyIntEquiv>::check(result))
                PyErr_SetObject(PyExc_OverflowError, 0);
            return static_cast<PyIntEquiv>(result);
        }
    }
};

template <typename T>
struct Converter_PyULongInt : Converter_PyInt<T>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<T*>(cppobj)); }
    static inline PyObject* toPython(const T& cppobj) { return PyLong_FromUnsignedLong(cppobj); }
};

/// Specialization to convert char and unsigned char, it accepts Python numbers and strings with just one character.
template <typename CharType>
struct CharConverter
{
    static inline bool checkType(PyObject* pyobj) { return SbkChar_Check(pyobj); }
    static inline bool isConvertible(PyObject* pyobj) { return SbkChar_Check(pyobj); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<CharType*>(cppobj)); }
    static inline PyObject* toPython(const CharType& cppobj) { return PyInt_FromLong(cppobj); }
    static CharType toCpp(PyObject* pyobj)
    {
        if (PyBytes_Check(pyobj)) {
            assert(PyBytes_GET_SIZE(pyobj) == 1); // This check is made on SbkChar_Check
            return PyBytes_AS_STRING(pyobj)[0];
        } else if (PyInt_Check(pyobj)) {
            PY_LONG_LONG result = PyInt_AsUnsignedLongLongMask(pyobj);
            if (OverFlowChecker<CharType>::check(result))
                PyErr_SetObject(PyExc_OverflowError, 0);
            return result;
        } else if (Shiboken::String::check(pyobj)) {
            return Shiboken::String::toCString(pyobj)[0];
        } else {
            return 0;
        }
    }
};

template <> struct Converter<unsigned long> : Converter_PyULongInt<unsigned long> {};
template <> struct Converter<unsigned int> : Converter_PyULongInt<unsigned int> {};
template <> struct Converter<char> : CharConverter<char>
{
    // Should we really return a string?
    using CharConverter<char>::toPython;
    using CharConverter<char>::isConvertible;
    using CharConverter<char>::toCpp;


    static inline bool isConvertible(PyObject* pyobj) {
        return SbkChar_Check(pyobj);
    }

    static inline PyObject* toPython(const char& cppObj) {
        return Shiboken::String::fromFormat("%c", cppObj);
    }

    static char toCpp(PyObject* pyobj)
    {
        if (PyBytes_Check(pyobj)) {
            assert(PyBytes_GET_SIZE(pyobj) == 1); // This check is made on SbkChar_Check
            return PyBytes_AS_STRING(pyobj)[0];
        } else if (PyInt_Check(pyobj)) {
            PY_LONG_LONG result = PyInt_AsUnsignedLongLongMask(pyobj);
            if (OverFlowChecker<char>::check(result))
                PyErr_SetObject(PyExc_OverflowError, 0);
            return result;
        } else if (Shiboken::String::check(pyobj)) {
            return Shiboken::String::toCString(pyobj)[0];
        } else {
            return 0;
        }
    }
};
template <> struct Converter<signed char> : CharConverter<signed char> {};
template <> struct Converter<unsigned char> : CharConverter<unsigned char> {};
template <> struct Converter<int> : Converter_PyInt<int> {};
template <> struct Converter<short> : Converter_PyInt<short> {};
template <> struct Converter<unsigned short> : Converter_PyInt<unsigned short> {};
template <> struct Converter<long> : Converter_PyInt<long> {};

template <>
struct Converter<PY_LONG_LONG>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<PY_LONG_LONG*>(cppobj)); }
    static inline PyObject* toPython(PY_LONG_LONG cppobj) { return PyLong_FromLongLong(cppobj); }
    static inline PY_LONG_LONG toCpp(PyObject* pyobj) { return (PY_LONG_LONG) PyLong_AsLongLong(pyobj); }
};

template <>
struct Converter<unsigned PY_LONG_LONG>
{
    static inline PyObject* toPython(void* cppobj)
    {
        return toPython(*reinterpret_cast<unsigned PY_LONG_LONG*>(cppobj));
    }
    static inline PyObject* toPython(unsigned PY_LONG_LONG cppobj)
    {
        return PyLong_FromUnsignedLongLong(cppobj);
    }
    static inline unsigned PY_LONG_LONG toCpp(PyObject* pyobj)
    {
        return (unsigned PY_LONG_LONG) PyLong_AsUnsignedLongLong(pyobj);
    }
};

template <typename PyFloatEquiv>
struct Converter_PyFloat
{
    static inline bool checkType(PyObject* obj) { return PyFloat_Check(obj); }
    static inline bool isConvertible(PyObject* obj) { return SbkNumber_Check(obj); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<PyFloatEquiv*>(cppobj)); }
    static inline PyObject* toPython(PyFloatEquiv cppobj) { return PyFloat_FromDouble((double) cppobj); }
    static inline PyFloatEquiv toCpp(PyObject* pyobj)
    {
        if (PyInt_Check(pyobj) || PyLong_Check(pyobj))
            return (PyFloatEquiv) PyLong_AsLong(pyobj);
        return (PyFloatEquiv) PyFloat_AsDouble(pyobj);
    }
};

template <> struct Converter<float> : Converter_PyFloat<float> {};
template <> struct Converter<double> : Converter_PyFloat<double> {};

// PyEnum Conversions ---------------------------------------------------------
template <typename CppEnum>
struct EnumConverter
{
    static inline bool checkType(PyObject* pyObj) { return PyObject_TypeCheck(pyObj, SbkType<CppEnum>()); }
    static inline bool isConvertible(PyObject* pyObj) { return PyObject_TypeCheck(pyObj, SbkType<CppEnum>()); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<CppEnum*>(cppobj)); }
    static inline PyObject* toPython(CppEnum cppenum)
    {
        return Shiboken::Enum::newItem(Shiboken::SbkType<CppEnum>(), (long) cppenum);
    }
    static inline CppEnum toCpp(PyObject* pyObj)
    {
        return (CppEnum) Shiboken::Enum::getValue(pyObj);;
    }
};

// C Sting Types --------------------------------------------------------------
template <typename CString>
struct Converter_CString
{
    // Note: 0 is also a const char* in C++, so None is accepted in checkType
    static inline bool checkType(PyObject* pyObj) {
        return Shiboken::String::check(pyObj);
    }
    static inline bool isConvertible(PyObject* pyObj) {
        return Shiboken::String::isConvertible(pyObj);
    }
    static inline PyObject* toPython(void* cppobj) { return toPython(reinterpret_cast<CString>(cppobj)); }
    static inline PyObject* toPython(CString cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        return Shiboken::String::fromCString(cppobj);
    }
    static inline CString toCpp(PyObject* pyobj) {
        if (pyobj == Py_None)
            return 0;
        return Shiboken::String::toCString(pyobj);
    }
};

template <> struct Converter<const char*> : Converter_CString<const char*> {};

template <> struct Converter<std::string> : Converter_CString<std::string>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<std::string*>(cppobj)); }
    static inline PyObject* toPython(std::string cppObj)
    {
        return Shiboken::String::fromCString(cppObj.c_str());
    }

    static inline std::string toCpp(PyObject* pyobj)
    {
        if (pyobj == Py_None)
            return 0;
        return std::string(Shiboken::String::toCString(pyobj));
    }
};

// C++ containers -------------------------------------------------------------
// The following container converters are meant to be used for pairs, lists and maps
// that are similar to the STL containers of the same name.

// For example to create a converter for a std::list the following code is enough:
// template<typename T> struct Converter<std::list<T> > : StdListConverter<std::list<T> > {};

// And this for a std::map:
// template<typename KT, typename VT>
// struct Converter<std::map<KT, VT> > : StdMapConverter<std::map<KT, VT> > {};

template <typename StdList>
struct StdListConverter
{
    static inline bool checkType(PyObject* pyObj)
    {
        return isConvertible(pyObj);
    }

    static inline bool isConvertible(PyObject* pyObj)
    {
        if (PyObject_TypeCheck(pyObj, SbkType<StdList>()))
            return true;
        // Sequence conversion are made ONLY for python sequences, not for
        // binded types implementing sequence protocol, otherwise this will
        // cause a mess like QBitArray being accepted by someone expecting a
        // QStringList.
        if ((SbkType<StdList>() && Object::checkType(pyObj)) || !PySequence_Check(pyObj))
            return false;
        for (int i = 0, max = PySequence_Length(pyObj); i < max; ++i) {
            AutoDecRef item(PySequence_GetItem(pyObj, i));
            if (!Converter<typename StdList::value_type>::isConvertible(item))
                return false;
        }
        return true;
    }
    static PyObject* toPython(void* cppObj) { return toPython(*reinterpret_cast<StdList*>(cppObj)); }
    static PyObject* toPython(const StdList& cppobj)
    {
        PyObject* result = PyList_New((int) cppobj.size());
        typename StdList::const_iterator it = cppobj.begin();
        for (int idx = 0; it != cppobj.end(); ++it, ++idx) {
            typename StdList::value_type vh(*it);
            PyList_SET_ITEM(result, idx, Converter<typename StdList::value_type>::toPython(vh));
        }
        return result;
    }
    static StdList toCpp(PyObject* pyobj)
    {
        if (PyObject_TypeCheck(pyobj, SbkType<StdList>()))
            return *reinterpret_cast<StdList*>(Object::cppPointer(reinterpret_cast<SbkObject*>(pyobj), SbkType<StdList>()));

        StdList result;
        for (int i = 0; i < PySequence_Size(pyobj); i++) {
            AutoDecRef pyItem(PySequence_GetItem(pyobj, i));
            result.push_back(Converter<typename StdList::value_type>::toCpp(pyItem));
        }
        return result;
    }
};

template <typename StdPair>
struct StdPairConverter
{
    static inline bool checkType(PyObject* pyObj)
    {
        return isConvertible(pyObj);
    }

    static inline bool isConvertible(PyObject* pyObj)
    {
        if (PyObject_TypeCheck(pyObj, SbkType<StdPair>()))
            return true;
        if ((SbkType<StdPair>() && Object::checkType(pyObj)) || !PySequence_Check(pyObj) || PySequence_Length(pyObj) != 2)
            return false;

        AutoDecRef item1(PySequence_GetItem(pyObj, 0));
        AutoDecRef item2(PySequence_GetItem(pyObj, 1));

        if (!Converter<typename StdPair::first_type>::isConvertible(item1)
            && !Converter<typename StdPair::second_type>::isConvertible(item2)) {
            return false;
        }
        return true;
    }
    static PyObject* toPython(void* cppObj) { return toPython(*reinterpret_cast<StdPair*>(cppObj)); }
    static PyObject* toPython(const StdPair& cppobj)
    {
        typename StdPair::first_type first(cppobj.first);
        typename StdPair::second_type second(cppobj.second);
        PyObject* tuple = PyTuple_New(2);
        PyTuple_SET_ITEM(tuple, 0, Converter<typename StdPair::first_type>::toPython(first));
        PyTuple_SET_ITEM(tuple, 1, Converter<typename StdPair::second_type>::toPython(second));
        return tuple;
    }
    static StdPair toCpp(PyObject* pyobj)
    {
        StdPair result;
        AutoDecRef pyFirst(PySequence_GetItem(pyobj, 0));
        AutoDecRef pySecond(PySequence_GetItem(pyobj, 1));
        result.first = Converter<typename StdPair::first_type>::toCpp(pyFirst);
        result.second = Converter<typename StdPair::second_type>::toCpp(pySecond);
        return result;
    }
};

template <typename StdMap>
struct StdMapConverter
{
    static inline bool checkType(PyObject* pyObj)
    {
        return isConvertible(pyObj);
    }

    static inline bool isConvertible(PyObject* pyObj)
    {
        if (PyObject_TypeCheck(pyObj, SbkType<StdMap>()))
            return true;
        if ((SbkType<StdMap>() && Object::checkType(pyObj)) || !PyDict_Check(pyObj))
            return false;

        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(pyObj, &pos, &key, &value)) {
            if (!Converter<typename StdMap::key_type>::isConvertible(key)
                || !Converter<typename StdMap::mapped_type>::isConvertible(value)) {
                return false;
            }
        }
        return true;
    }

    static PyObject* toPython(void* cppObj) { return toPython(*reinterpret_cast<StdMap*>(cppObj)); }
    static PyObject* toPython(const StdMap& cppobj)
    {
        PyObject* result = PyDict_New();
        typename StdMap::const_iterator it = cppobj.begin();

        for (; it != cppobj.end(); ++it) {
            PyDict_SetItem(result,
                           Converter<typename StdMap::key_type>::toPython(it->first),
                           Converter<typename StdMap::mapped_type>::toPython(it->second));
        }

        return result;
    }
    static StdMap toCpp(PyObject* pyobj)
    {
        StdMap result;

        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(pyobj, &pos, &key, &value)) {
            result.insert(typename StdMap::value_type(
                    Converter<typename StdMap::key_type>::toCpp(key),
                    Converter<typename StdMap::mapped_type>::toCpp(value)));
        }
        return result;
    }
};


// class used to translate python objects to another type
template <typename T> struct PythonConverter {};

} // namespace Shiboken

#endif // CONVERSIONS_H

