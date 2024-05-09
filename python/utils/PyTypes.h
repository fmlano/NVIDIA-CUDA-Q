/****************************************************************-*- C++ -*-****
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once

#include <complex>
#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <iostream>

namespace py = pybind11;

namespace py_ext {

/// Extended python complex object.
///
/// Includes `complex`, `numpy.complex64`, `numpy.complex128`.
class Complex : public py::object {
public:
  PYBIND11_OBJECT_CVT(Complex, object, isComplex_, convert_)

  Complex(double real, double imag)
      : object(PyComplex_FromDoubles(real, imag), stolen_t{}) {
    if (!m_ptr) {
      py::pybind11_fail("Could not allocate complex object!");
    }
  }

  // Allow implicit conversion from complex<double>/complex<float>:
  // NOLINTNEXTLINE(google-explicit-constructor)
  Complex(std::complex<double> value)
      : Complex((double)value.real(), (double)value.imag()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Complex(std::complex<float> value)
      : Complex((double)value.real(), (double)value.imag()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator std::complex<double>() {
    auto value = PyComplex_AsCComplex(m_ptr);
    return std::complex<double>(value.real, value.imag);
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator std::complex<float>() {
    auto value = PyComplex_AsCComplex(m_ptr);
    return std::complex<float>(value.real, value.imag);
  }

  static bool isComplex_(PyObject *o) {
    if (PyComplex_Check(o)) {
      return true;
    }
    PyTypeObject *type = Py_TYPE(o);
    std::string name = std::string(type->tp_name);
    if (name == "numpy.complex64" || name == "numpy.complex128") {
      return true;
    }
    return false;
  }

  static PyObject *convert_(PyObject *o) {
    PyObject *ret = nullptr;
    if (isComplex_(o)) {
      double real = PyComplex_RealAsDouble(o);
      double imag = PyComplex_ImagAsDouble(o);
      ret = PyComplex_FromDoubles(real, imag);
    } else {
      py::set_error(PyExc_TypeError, "Unexpected type");
    }
    return ret;
  }
};

/// Extended python float object.
///
/// Includes `float`, `numpy.float64`, `numpy.float32`.
class Float : public py::object {
public:
  PYBIND11_OBJECT_CVT(Float, object, isFloat_, convert_)

  // Allow implicit conversion from float/double:
  // NOLINTNEXTLINE(google-explicit-constructor)
  Float(float value) : object(PyFloat_FromDouble((double)value), stolen_t{}) {
    if (!m_ptr) {
      py::pybind11_fail("Could not allocate float object!");
    }
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  Float(double value = .0)
      : object(PyFloat_FromDouble((double)value), stolen_t{}) {
    if (!m_ptr) {
      py::pybind11_fail("Could not allocate float object!");
    }
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator float() const { return (float)PyFloat_AsDouble(m_ptr); }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator double() const { return (double)PyFloat_AsDouble(m_ptr); }

  static bool isFloat_(PyObject *o) {
    if (PyFloat_Check(o)) {
      return true;
    }
    PyTypeObject *type = Py_TYPE(o);
    std::string name = std::string(type->tp_name);
    if (name == "numpy.float32" || name == "numpy.float64") {
      return true;
    }
    return false;
  }

  static PyObject *convert_(PyObject *o) {
    PyObject *ret = nullptr;
    if (isFloat_(o)) {
      ret = PyFloat_FromDouble(PyFloat_AsDouble(o));
    } else {
      py::set_error(PyExc_TypeError, "Unexpected type");
    }
    return ret;
  }
};

template <typename T>
inline std::string typeName() {
  return {typeid(T).name()};
}
template <>
inline std::string typeName<py_ext::Float>() {
  return "float";
}
template <>
inline std::string typeName<py_ext::Complex>() {
  return "complex";
}
template <>
inline std::string typeName<py::int_>() {
  return "int";
}
template <>
inline std::string typeName<py::bool_>() {
  return "bool";
}
template <>
inline std::string typeName<py::list>() {
  return "list";
}

template <typename T, py::detail::enable_if_t<
                          std::is_base_of<py::object, T>::value, int> = 0>
inline bool isConvertible(py::handle o) {
  return py::isinstance<T>(o);
}
template <>
inline bool isConvertible<Complex>(py::handle o) {
  return py::isinstance<Complex>(o) || py::isinstance<Float>(o) ||
         py::isinstance<py::int_>(o);
}
template <>
inline bool isConvertible<Float>(py::handle o) {
  return py::isinstance<Float>(o) || py::isinstance<py::int_>(o);
}
} // namespace py_ext
