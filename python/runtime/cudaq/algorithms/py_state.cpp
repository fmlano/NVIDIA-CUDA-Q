/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#include <pybind11/complex.h>
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
      "A data-type representing the quantum state of the internal simulator. "
      "Returns state vector by default. If qpu is set to `density-matrix-cpu`, "
      "returns "
      "density matrix.\n")
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
        return py::buffer_info(
            self.get_data(), sizeof(std::complex<double>), /*itemsize */
            py::format_descriptor<std::complex<double>>::format(), 1, /* ndim */
            {shape[0]}, /* shape */
            {sizeof(std::complex<double>)});
      })
      .def(py::init([](const py::buffer &b) {
             py::buffer_info info = b.request();
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
           "Construct the :class:`State` from an existing array of data.")
      .def(
          "__getitem__", [](state &s, std::size_t idx) { return s[idx]; },
          "Return the `index`-th element of the state vector.")
      .def(
          "__getitem__",
          [](state &s, std::vector<std::size_t> idx) {
            return s(idx[0], idx[1]);
          },
          "Return a matrix element of the density matrix.")
      .def(
          "dump",
          [](state &self) {
            std::stringstream ss;
            self.dump(ss);
            py::print(ss.str());
          },
          "Print the state to standard out")
      .def("__str__",
           [](state &self) {
             std::stringstream ss;
             self.dump(ss);
             return ss.str();
           })
      .def(
          "overlap", [](state &s, state &other) { return s.overlap(other); },
          "Compute the overlap of this state with the other one.")
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
          "Compute the overlap of this state with the other one.");

  mod.def(
      "get_state",
      [](kernel_builder<> &kernel, py::args args) {
        return pyGetState(kernel, args);
      },
      "Return the state generated by the given quantum kernel.");

  py::class_<async_state_result>(
      mod, "AsyncStateResult",
      R"#(A data-type containing the results of a call to :func:`get_state_async`. 
The `AsyncStateResult` models a future-like type, whose 
:class:`State` may be returned via an invocation of the `get` method. This 
kicks off a wait on the current thread until the results are available.
See `future <https://en.cppreference.com/w/cpp/thread/future>`_ 
for more information on this programming pattern.)#")
      .def(
          "get", [](async_state_result &self) { return self.get(); },
          "Return the :class:`State` from the asynchronous `get_state` "
          "accessor execution.\n");

  mod.def(
      "get_state_async",
      [](kernel_builder<> &kernel, py::args args, std::size_t qpu_id) {
        // Ensure the user input is correct.
        auto validatedArgs = validateInputArguments(kernel, args);
        auto &platform = cudaq::get_platform();
        kernel.jitCode();
        return cudaq::details::runGetStateAsync(
            [&, a = std::move(validatedArgs)]() mutable {
              OpaqueArguments argData;
              packArgs(argData, a);
              kernel.jitAndInvoke(argData.data());
            },
            platform, qpu_id);
      },
      py::arg("kernel"), py::kw_only(), py::arg("qpu_id") = 0,
      R"#(Asynchronously retrieve the state generated by the given quantum kernel. 
When targeting a quantum platform with more than one QPU, the optional
`qpu_id` allows for control over which QPU to enable. Will return a
future whose results can be retrieved via `future.get()`.

Args:
  kernel (:class:`Kernel`): The :class:`Kernel` to execute on the QPU.
  *arguments (Optional[Any]): The concrete values to evaluate the kernel 
    function at. Leave empty if the kernel doesn't accept any arguments.
  qpu_id (Optional[int]): The optional identification for which QPU 
    on the platform to target. Defaults to zero. Key-word only.

Returns:
  :class:`AsyncStateResult`: Quantum state (state vector or density matrix) data).)#");
}

} // namespace cudaq
