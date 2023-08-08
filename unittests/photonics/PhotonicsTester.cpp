/*******************************************************************************
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include <gtest/gtest.h>

#include "cudaq.h"
#include "cudaq/photonics.h"

TEST(PhotonicsTester, checkSimple) {

  struct test {
    auto operator()() __qpu__ {
      cudaq::qreg<cudaq::dyn, 3> qutrits(2);
      plusGate(qutrits[0]);
      plusGate(qutrits[1]);
      plusGate(qutrits[1]);
      return mz(qutrits);
    }
  };

  struct test2 {
    void operator()() __qpu__ {
      cudaq::qreg<cudaq::dyn, 3> qutrits(2);
      plusGate(qutrits[0]);
      plusGate(qutrits[1]);
      plusGate(qutrits[1]);
      mz(qutrits);
    }
  };

  auto res = test{}();
  EXPECT_EQ(res[0], 1);
  EXPECT_EQ(res[1], 2);

  auto counts = cudaq::sample(test2{});
  for (auto &[k, v] : counts) {
    printf("Result / Count = %s : %lu\n", k.c_str(), v);
  }
}

TEST(PhotonicsTester, checkHOM) {

  struct HOM {
    // Hong–Ou–Mandel effect
    auto operator()() __qpu__ {

      constexpr std::array<std::size_t, 2> input_state{1, 1};

      cudaq::qreg<cudaq::dyn, 3> quds(2); // |00>
      for (std::size_t i = 0; i < 2; i++) {
        for (std::size_t j = 0; j < input_state[i]; j++) {
          plusGate(quds[i]); // setting to  |11>
        }
      }

      beamSplitterGate(quds[0], quds[1], M_PI / 4);
      mz(quds);
    }
  };

  struct HOM2 {
    // Hong–Ou–Mandel effect
    auto operator()() __qpu__ {

      constexpr std::array<std::size_t, 2> input_state{1, 1};

      cudaq::qreg<cudaq::dyn, 3> quds(2); // |00>
      for (std::size_t i = 0; i < 2; i++) {
        for (std::size_t j = 0; j < input_state[i]; j++) {
          plusGate(quds[i]); // setting to  |11>
        }
      }

      beamSplitterGate(quds[0], quds[1], M_PI / 6);
      mz(quds);
    }
  };

  auto counts = cudaq::sample(HOM{});
  EXPECT_EQ(counts.size(), 2);

  auto counts2 = cudaq::sample(HOM2{});
  EXPECT_EQ(counts2.size(), 3);
}

TEST(PhotonicsTester, checkMZI) {

  struct MZI {
    // Mach-Zendher Interferometer
    auto operator()() __qpu__ {

      constexpr std::array<std::size_t, 2> input_state{1, 0};

      cudaq::qreg<cudaq::dyn, 3> quds(2); // |00>
      for (std::size_t i = 0; i < 2; i++) {
        for (std::size_t j = 0; j < input_state[i]; j++) {
          plusGate(quds[i]); // setting to  |10>
        }
      }

      beamSplitterGate(quds[0], quds[1], M_PI / 4);
      phaseShiftGate(quds[0], M_PI / 3);

      beamSplitterGate(quds[0], quds[1], M_PI / 4);
      phaseShiftGate(quds[0], M_PI / 3);

      mz(quds);
    }
  };

  std::size_t shots = 1000000;
  auto counts = cudaq::sample(shots, MZI{}); //

  EXPECT_NEAR(double(counts.count("10")) / shots, cos(M_PI / 3) * cos(M_PI / 3),
              1e-3);
}