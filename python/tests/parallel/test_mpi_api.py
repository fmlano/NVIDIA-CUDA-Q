# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

import os, pytest, importlib
import cudaq
import numpy as np

skipIfUnsupported = pytest.mark.skipif(importlib.util.find_spec('mpi4py')
                                       is None,
                                       reason="mpi4py not found")


@skipIfUnsupported
def testMPI():
    cudaq.mpi.initialize()
    assert cudaq.mpi.is_initialized() == True
    # Check rank API
    if os.environ.get('OMPI_COMM_WORLD_RANK') is not None:
        print("Rank:", os.environ.get('OMPI_COMM_WORLD_RANK'))
        assert cudaq.mpi.rank() == int(os.environ.get('OMPI_COMM_WORLD_RANK'))

    if os.environ.get('OMPI_COMM_WORLD_SIZE') is not None:
        assert cudaq.mpi.num_ranks() == int(
            os.environ.get('OMPI_COMM_WORLD_SIZE'))

    localData = [float(cudaq.mpi.rank())]
    gatherData = cudaq.mpi.all_gather(cudaq.mpi.num_ranks(), localData)
    assert len(gatherData) == cudaq.mpi.num_ranks()
    for idx, x in enumerate(gatherData):
        assert abs(gatherData[idx] - float(idx)) < 1e-12
    cudaq.mpi.finalize()


# leave for gdb debugging
if __name__ == "__main__":
    loc = os.path.abspath(__file__)
    pytest.main([loc, "-s"])
