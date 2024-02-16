import cudaq

# This example assumes the NVQC API key and Function Id have been set in the `~/.nvqc_config` file/environment variables.
# If not, you can set the API Key and Function ID environment variables in the Python script with:
# ```
# os.environ["NVQC_API_KEY"] = "<YOUR NVQC API KEY>"`
# os.environ["NVQC_FUNCTION_ID"] = "<YOUR NVQC FUNCTION ID>"
# ```
# Alternatively, the `api_key` and `function_id` values can be passed to the target directly,
# ```
# cudaq.set_target("nvqc",
#                 api_key="<YOUR NVQC API KEY>"
#                 function_id="<YOUR NVQC FUNCTION ID>")
# ```
cudaq.set_target("nvqc")

num_qubits = 20
kernel = cudaq.make_kernel()
qubits = kernel.qalloc(num_qubits)
# Place qubits in GHZ state.
kernel.h(qubits[0])
for i in range(num_qubits - 1):
    kernel.cx(qubits[i], qubits[i + 1])

state = cudaq.get_state(kernel)
print("Amplitude(00..00) =", state[0])
print("Amplitude(11..11) =", state[2**num_qubits - 1])
