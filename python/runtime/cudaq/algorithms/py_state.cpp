/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/
#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include "py_observe.h"
#include "py_state.h"
#include "utils/OpaqueArguments.h"

#include "cudaq/algorithms/state.h"

namespace cudaq {

/// @brief Extract the state data
void extractStateData(py::buffer_info &info, complex *data) {
  if (info.format != py::format_descriptor<complex>::format())
    throw std::runtime_error(
        "Incompatible buffer format, must be np.complex128.");

  if (info.ndim > 2)
    throw std::runtime_error("Incompatible buffer shape.");

  std::size_t size = info.shape[0];
  if (info.shape.size() == 2)
    size *= info.shape[1];
  memcpy(data, info.ptr, sizeof(complex) * (size));
}

/// @brief Run `cudaq::get_state` on the provided kernel and spin operator.
state pyGetState(kernel_builder<> &kernel, py::args args) {
  // Ensure the user input is correct.
  auto validatedArgs = validateInputArguments(kernel, args);
  kernel.jitCode();
  OpaqueArguments argData;
  packArgs(argData, validatedArgs);
  return details::extractState(
      [&]() mutable { kernel.jitAndInvoke(argData.data()); });
}

/// @brief Bind the get_state cudaq function
void bindPyState(py::module &mod) {

  py::class_<state>(
      mod, "State", py::buffer_protocol(),
      "A data-type representing the quantum state of the interal simulator. "
      "Returns state vector by default. If qpu is set to `dm`, returns "
      "density matrix.\n")
      // Define a buffer model for this class to make it fully
      // compatible with numpy.
      .def_buffer([](state &self) -> py::buffer_info {
        auto shape = self.get_shape();
        if (shape.size() != 1) {
          return py::buffer_info(
              self.get_data(), sizeof(std::complex<double>), /*itemsize */
              py::format_descriptor<std::complex<double>>::format(),
              2,                    /* ndim */
              {shape[0], shape[1]}, /* shape */
              {sizeof(std::complex<double>) * shape[1],
               sizeof(std::complex<double>)}, /* strides */
              true                            /* readonly */
          );
        }
        // If 1-dimension, return the state vector.
        return py::buffer_info(
            self.get_data(), sizeof(std::complex<double>), /*itemsize */
            py::format_descriptor<std::complex<double>>::format(), 1, /* ndim */
            {shape[0]},                                /* shape */
            {sizeof(std::complex<double>) * shape[0]}, /* strides */
            true                                       /* readonly */
        );
      })
      .def(py::init([](const py::buffer &buffer) {
             py::buffer_info info = buffer.request();
             std::vector<std::size_t> shape;
             for (auto s : info.shape)
               shape.push_back(s);
             std::size_t size = shape[0];
             if (shape.size() == 2)
               size *= shape[1];
             std::vector<complex> v(size);
             extractStateData(info, v.data());
             auto t = std::make_tuple(shape, v);
             return state(t);
           }),
           "Construct the :class:`State` from an existing array of data.\n")
      .def(
          "__getitem__", [](state &self, std::size_t idx) { return self[idx]; },
          "Return the `index`-th element of the state vector.")
      .def(
          "__getitem__",
          [](state &self, std::vector<std::size_t> idx) {
            return self(idx[0], idx[1]);
          },
          "Return a matrix element of the density matrix.")
      .def(
          "dump",
          [](state &self) {
            std::stringstream ss;
            self.dump(ss);
            py::print(ss.str());
          },
          "Print the state to the terminal.")
      .def("__str__",
           [](state &self) {
             std::stringstream ss;
             self.dump(ss);
             return ss.str();
           })
      .def(
          "overlap",
          [](state &self, state &other) { return self.overlap(other); },
          "Compute the overlap of `self` with the `other` :class:`State`.")
      .def(
          "overlap",
          [](state &self, py::buffer &other) {
            py::buffer_info info = other.request();
            std::vector<std::size_t> shape;
            for (auto s : info.shape)
              shape.push_back(s);
            std::size_t size = shape[0];
            if (shape.size() == 2)
              size *= shape[1];
            std::vector<complex> v(size);
            extractStateData(info, v.data());
            auto t = std::make_tuple(shape, v);
            state ss(t);
            return self.overlap(ss);
          },
          "Compute the overlap of `self` with `other`.");

  mod.def(
      "get_state",
      [](kernel_builder<> &kernel, py::args arguments) {
        return pyGetState(kernel, arguments);
      },
      R"(Simulate the given :class:`Kernel` and return its quantum state.

      Args:
        kernel (:class:`Kernel`) : The kernel to return the quantum state of.
        *arguments (Optional[Any]) : The concrete arguments to the provided kernel. Leave empty if the kernel doesn't acccept arguments.

      Returns:
        :class:`State` : The quantum state represented as a :class:`State` data-type. If the qpu is set to `dm`, this will be a density matrix. By default, it will return a state vector.
      )");
}

} // namespace cudaq
