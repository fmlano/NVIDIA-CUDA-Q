/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#include <cudaq/spin_op.h>
#include <stdint.h>
#ifdef CUDAQ_HAS_OPENMP
#include <omp.h>
#endif

#include <Eigen/Dense>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <complex>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <utility>
#include <vector>

namespace cudaq {

/// @brief Compute the action
/// @param term
/// @param bitConfiguration
/// @return
std::pair<std::string, std::complex<double>>
actionOnBra(spin_op &term, const std::string &bitConfiguration) {
  auto coeff = term.get_coefficient();
  auto newConfiguration = bitConfiguration;
  std::complex<double> i(0, 1);

  term.for_each_pauli([&](pauli p, std::size_t idx) {
    if (p == pauli::Z) {
      coeff *= (newConfiguration[idx] == '1' ? -1 : 1);
    } else if (p == pauli::X) {
      newConfiguration[idx] = newConfiguration[idx] == '1' ? '0' : '1';
    } else if (p == pauli::Y) {
      coeff *= (newConfiguration[idx] == '1' ? i : -i);
      newConfiguration[idx] = (newConfiguration[idx] == '1' ? '0' : '1');
    }
  });

  return std::make_pair(newConfiguration, coeff);
}

complex_matrix spin_op::to_matrix() const {
  auto n = n_qubits();
  auto dim = 1UL << n;
  auto getBitStrForIdx = [&](std::size_t i) {
    std::stringstream s;
    for (int k = n - 1; k >= 0; k--)
      s << ((i >> k) & 1);
    return s.str();
  };

  // To construct the matrix, we are looping over every
  // row, computing the binary representation for that index,
  // e.g <100110|, and then we will compute the action of
  // each pauli term on that binary configuration, returning a new
  // product state and coefficient. Call this new state <colState|,
  // we then compute <rowState | Paulis | colState> and set it in the matrix
  // data.

  complex_matrix A(dim, dim);
  A.set_zero();
  auto rawData = A.data();
#pragma omp parallel for shared(rawData)
  for (std::size_t rowIdx = 0; rowIdx < dim; rowIdx++) {
    auto rowBitStr = getBitStrForIdx(rowIdx);
    for_each_term([&](spin_op &term) {
      auto [res, coeff] = actionOnBra(term, rowBitStr);
      auto colIdx = std::stol(res, nullptr, 2);
      rawData[rowIdx * dim + colIdx] += coeff;
    });
  }
  return A;
}

std::complex<double> spin_op::get_coefficient() const {
  if (terms.size() != 1)
    throw std::runtime_error(
        "spin_op::get_coefficient called on spin_op with > 1 terms.");
  return terms.begin()->second;
}

void spin_op::for_each_term(std::function<void(spin_op &)> &&functor) const {
  for (std::size_t i = 0; i < n_terms(); i++) {
    auto term = operator[](i);
    functor(term);
  }
}
void spin_op::for_each_pauli(
    std::function<void(pauli, std::size_t)> &&functor) const {
  if (n_terms() != 1)
    throw std::runtime_error(
        "spin_op::for_each_pauli on valid for spin_op with n_terms == 1.");

  auto nQ = n_qubits();
  auto bsf = terms.begin()->first;
  for (std::size_t i = 0; i < nQ; i++) {
    if (bsf[i] && bsf[i + nQ]) {
      functor(pauli::Y, i);
    } else if (bsf[i]) {
      functor(pauli::X, i);
    } else if (bsf[i + nQ]) {
      functor(pauli::Z, i);
    } else {
      functor(pauli::I, i);
    }
  }
}

spin_op spin_op::random(std::size_t nQubits, std::size_t nTerms) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::vector<std::complex<double>> coeff(nTerms, 1.0);
  std::vector<std::vector<bool>> randomTerms;
  for (std::size_t i = 0; i < nTerms; i++) {
    std::vector<bool> termData(2 * nQubits);
    std::fill_n(termData.begin(), termData.size() * (1 - .5), 1);
    std::shuffle(termData.begin(), termData.end(), gen);
    randomTerms.push_back(termData);
  }

  return cudaq::spin_op::from_binary_symplectic(randomTerms, coeff);
}

void spin_op::expandToNQubits(const std::size_t numQubits) {
  auto iter = terms.begin();
  while (iter != terms.end()) {
    auto coeff = iter->second;
    std::vector<bool> tmp = iter->first;
    if (tmp.size() == numQubits * 2) {
      iter++;
      continue;
    }

    auto newSize = numQubits * 2 - tmp.size();
    for (std::size_t i = 0; i < newSize / 2; i++) {
      tmp.insert(tmp.begin() + tmp.size() / 2, 0);
      tmp.insert(tmp.begin() + tmp.size(), 0);
    }

    terms.erase(iter++);
    terms.emplace(tmp, coeff);
  }
}

spin_op::spin_op() {
  std::vector<bool> init(2);
  terms.emplace(init, 1.0);
}

spin_op::spin_op(std::size_t numQubits) {
  std::vector<bool> init(2 * numQubits);
  terms.emplace(init, 1.0);
}

spin_op::spin_op(BinarySymplecticForm d,
                 std::vector<std::complex<double>> coeffs) {
  m_n_qubits = d[0].size() / 2.;
  for (std::size_t i = 0; auto &t : d)
    terms.emplace(t, coeffs[i++]);
}

spin_op::spin_op(pauli type, const std::size_t idx,
                 std::complex<double> coeff) {
  m_n_qubits = idx + 1;
  std::vector<bool> d(2 * m_n_qubits);

  if (type == pauli::X)
    d[idx] = 1;
  else if (type == pauli::Y) {
    d[idx] = 1;
    d[idx + m_n_qubits] = 1;
  } else if (type == pauli::Z)
    d[idx + m_n_qubits] = 1;

  terms.emplace(d, coeff);
}

spin_op::spin_op(const spin_op &o) : terms(o.terms) {}

spin_op &spin_op::operator+=(const spin_op &v) noexcept {
  auto otherNumQubits = v.n_qubits();

  spin_op tmpv = v;
  if (otherNumQubits > n_qubits())
    expandToNQubits(otherNumQubits);
  else if (otherNumQubits < n_qubits())
    tmpv.expandToNQubits(n_qubits());

  for (auto [term, coeff] : tmpv.terms) {
    // std::cout << "Adding\n" << coeff << " ";
    // for (auto a : term)
    //   std::cout << a << " ";
    // std::cout << " to " << to_string();
    auto iter = terms.find(term);
    if (iter != terms.end())
      iter->second += coeff;
    else
      terms.emplace(term, coeff);

    // std::cout << "Result is\n" << to_string();
  }

  return *this;
}

// std::vector<std::complex<double>> spin_op::get_coefficien2s() const {
//   // do we need this now?
//   // throw std::runtime_error("get_coefficien2ts does not work.");
//   std::vector<std::complex<double>> coeffs;
//   for (auto &[term, c] : terms)
//     coeffs.push_back(c);

//   return coeffs;
// }

spin_op spin_op::operator[](const std::size_t term_idx) const {
  auto start = terms.begin();
  std::advance(start, term_idx);
  return spin_op(*start);
}

spin_op &spin_op::operator-=(const spin_op &v) noexcept {
  return operator+=(-1.0 * v);
}

spin_op::spin_op(std::pair<std::vector<bool>, std::complex<double>> term) {
  terms.emplace(term.first, term.second);
}

std::pair<std::complex<double>, std::vector<bool>>
mult(std::vector<bool> row, std::vector<bool> other_row,
     std::complex<double> &rowCoeff, std::complex<double> &otherCoeff) {
  // This is term_i * otherTerm_j
  std::vector<bool> tmp(row.size()), tmp2(row.size());
  std::size_t m_n_qubits = row.size() / 2;

  for (std::size_t i = 0; i < 2 * m_n_qubits; i++)
    tmp[i] = row[i] ^ other_row[i];

  for (std::size_t i = 0; i < m_n_qubits; i++)
    tmp2[i] = row[i] && other_row[m_n_qubits + i];

  int orig_phase = 0, other_phase = 0;
  for (std::size_t i = 0; i < m_n_qubits; i++) {
    if (row[i] && row[i + m_n_qubits])
      orig_phase++;

    if (other_row[i] && other_row[i + m_n_qubits])
      other_phase++;
  }

  auto _phase = orig_phase + other_phase;
  int sum = 0;
  for (auto a : tmp2)
    if (a)
      sum++;

  _phase += 2 * sum;
  // Based on the phase, figure out an extra coeff to apply
  for (std::size_t i = 0; i < m_n_qubits; i++)
    if (tmp[i] && tmp[i + m_n_qubits])
      _phase -= 1;

  _phase %= 4;
  std::complex<double> imaginary(0, 1);
  std::map<int, std::complex<double>> phase_coeff_map{
      {0, 1.0}, {1, -1. * imaginary}, {2, -1.0}, {3, imaginary}};
  auto phase_coeff = phase_coeff_map[_phase];

  auto coeff = rowCoeff;
  coeff *= phase_coeff * otherCoeff;

  return std::make_pair(coeff, tmp);
}

spin_op &spin_op::operator*=(const spin_op &v) noexcept {
  spin_op copy = v;
  auto t1 = std::chrono::high_resolution_clock::now();
  // spin_op tmpv = v;
  if (v.n_qubits() > n_qubits())
    expandToNQubits(copy.n_qubits());
  else if (v.n_qubits() < n_qubits())
    copy.expandToNQubits(n_qubits());
  auto t2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> ms_double = t2 - t1;
  // std::cout << "Time block 0: " << ms_double.count() * 1e-3 << "\n";

  std::unordered_map<std::vector<bool>, std::complex<double>> newTerms;
  std::size_t ourRow = 0, theirRow = 0;
  std::vector<std::complex<double>> composedCoeffs(n_terms() * copy.n_terms());
  std::vector<std::vector<bool>> composition(n_terms() * copy.n_terms());
  std::map<std::size_t, std::pair<std::size_t, std::size_t>> indexMap;
  auto nElements = composition.size();
  t1 = std::chrono::high_resolution_clock::now();

  for (std::size_t i = 0; i < nElements; i++) {
    auto pair = std::make_pair(ourRow, theirRow);
    indexMap.emplace(i, pair);
    if (theirRow == copy.n_terms() - 1) {
      theirRow = 0;
      ourRow++;
    } else
      theirRow++;
  }
  t2 = std::chrono::high_resolution_clock::now();
  ms_double = t2 - t1;
  // std::cout << "Time block 1: " << ms_double.count() * 1e-3 << "\n";

  t1 = std::chrono::high_resolution_clock::now();
  // printf("Perform hard part\n");
#pragma omp parallel for shared(composition)
  for (std::size_t i = 0; i < nElements; i++) {
    auto [j, k] = indexMap[i];
    auto s = terms.begin();
    auto t = copy.terms.begin();
    std::advance(s, j);
    std::advance(t, k);
    auto res = mult(s->first, t->first, s->second, t->second);
    composition[i] = res.second;
    composedCoeffs[i] = res.first;
  }

  t2 = std::chrono::high_resolution_clock::now();
  ms_double = t2 - t1;
  // std::cout << "Time block 2: " << ms_double.count() * 1e-3 << "\n";

  t1 = std::chrono::high_resolution_clock::now();
  for (std::size_t i = 0; i < nElements; i++) {
    auto iter = newTerms.find(composition[i]);
    if (iter == newTerms.end())
      newTerms.emplace(composition[i], composedCoeffs[i]);
    else
      iter->second += composedCoeffs[i];
  }

  t2 = std::chrono::high_resolution_clock::now();
  ms_double = t2 - t1;
  // std::cout << "Time block 3: " << ms_double.count() * 1e-3 << "\n";

  t1 = std::chrono::high_resolution_clock::now();

  terms = newTerms;

  t2 = std::chrono::high_resolution_clock::now();
  ms_double = t2 - t1;
  std::cout << "Time block 4: " << ms_double.count() * 1e-3 << "\n";
  return *this;
}

bool spin_op::is_identity() const {
  for (auto &[row, c] : terms)
    for (auto e : row)
      if (e)
        return false;

  return true;
}

bool spin_op::operator==(const spin_op &v) const noexcept {
  // Could be that the term is identity with all zeros
  bool isId1 = true, isId2 = true;
  for (auto &[row, c] : terms)
    for (auto e : row)
      if (e) {
        isId1 = false;
        break;
      }

  for (auto &[row, c] : v.terms)
    for (auto e : row)
      if (e) {
        isId2 = false;
        break;
      }

  if (isId1 && isId2)
    return true;

  for (auto &[k, c] : terms) {
    if (v.terms.find(k) == v.terms.end())
      return false;
  }
  return true; // data == v.data;
}

spin_op &spin_op::operator*=(const double v) noexcept {
  for (auto &[term, coeff] : terms)
    coeff *= v;

  return *this;
}

spin_op &spin_op::operator*=(const std::complex<double> v) noexcept {
  for (auto &[term, coeff] : terms)
    coeff *= v;

  return *this;
}

std::size_t spin_op::n_qubits() const {
  if (terms.empty())
    return 0;
  return terms.begin()->first.size() / 2;
}
std::size_t spin_op::n_terms() const { return terms.size(); }

// std::complex<double>
// spin_op::get_term_coefficieent(const std::size_t idx) const {
//   auto start = terms.begin();
//   std::advance(start, idx);
//   return start->second;
// }

spin_op spin_op::slice(const std::size_t startIdx, const std::size_t count) {
  auto nTerms = n_terms();
  if (nTerms <= count)
    throw std::runtime_error("Cannot request slice with " +
                             std::to_string(count) + " terms on spin_op with " +
                             std::to_string(nTerms) + " terms.");
  // auto start = terms.begin();
  //   std::advance(start, startIdx);

  // for (auto iter = start; iter !=  )

  std::vector<std::complex<double>> newCoeffs;
  BinarySymplecticForm newData;
  // for (std::size_t i = startIdx; i < startIdx + count; ++i) {
  //   if (i == data.size())
  //     break;
  //   newData.push_back(data[i]);
  //   newCoeffs.push_back(coefficients[i]);
  // }
  return spin_op(newData, newCoeffs);
}

std::string spin_op::to_string(bool printCoeffs) const {
  std::stringstream ss;
  std::vector<std::string> printOut;
  for (auto &[term, coeff] : terms) {
    for (std::size_t i = 0; i < term.size() / 2; i++) {
      if (term[i] && term[i + term.size() / 2])
        printOut.push_back("Y");
      else if (term[i])
        printOut.push_back("X");
      else if (term[i + term.size() / 2])
        printOut.push_back("Z");
      else
        printOut.push_back("I");
    }

    if (printCoeffs)
      ss << fmt::format("[{}{}{}j]", coeff.real(),
                        coeff.imag() < 0.0 ? "-" : "+", std::fabs(coeff.imag()))
         << " ";
    
    ss << fmt::format("{}", fmt::join(printOut, ""));
    
    if (printCoeffs)
      ss << "\n";
    printOut.clear();
  }

  return ss.str();
}

void spin_op::dump() const {
  auto str = to_string();
  std::cout << str;
}

spin_op::spin_op(std::vector<double> &input_vec, std::size_t nQubits) {
  auto n_terms = (int)input_vec.back();
  if (nQubits != ((input_vec.size() - 2 * n_terms) / n_terms))
    throw std::runtime_error("Invalid data representation for construction "
                             "spin_op. Number of data elements is incorrect.");

  for (std::size_t i = 0; i < input_vec.size() - 1; i += nQubits + 2) {
    std::vector<bool> tmpv(2 * nQubits);
    for (std::size_t j = 0; j < nQubits; j++) {
      double intPart;
      if (std::modf(input_vec[j + i], &intPart) != 0.0)
        throw std::runtime_error(
            "Invalid pauli data element, must be integer value.");

      int val = (int)input_vec[j + i];
      if (val == 1) { // X
        tmpv[j] = 1;
      } else if (val == 2) { // Z
        tmpv[j + nQubits] = 1;
      } else if (val == 3) { // Y
        tmpv[j + nQubits] = 1;
        tmpv[j] = 1;
      }
    }
    auto el_real = input_vec[i + nQubits];
    auto el_imag = input_vec[i + nQubits + 1];
    terms.emplace(tmpv, std::complex<double>{el_real, el_imag});
  }
}

std::pair<spin_op::BinarySymplecticForm, std::vector<std::complex<double>>>
spin_op::get_bsf() const {
  BinarySymplecticForm data;
  std::vector<std::complex<double>> coeffs;
  for (auto &[term, c] : terms) {
    data.push_back(term);
    coeffs.push_back(c);
  }

  return std::make_pair(data, coeffs);
}

spin_op &spin_op::operator=(const spin_op &other) {
  terms = other.terms;
  return *this;
}

spin_op operator+(double coeff, spin_op op) {
  return spin_op(op.n_qubits()) * coeff + op;
}
spin_op operator+(spin_op op, double coeff) {
  return op + spin_op(op.n_qubits()) * coeff;
}
spin_op operator-(double coeff, spin_op op) {
  return spin_op(op.n_qubits()) * coeff - op;
}
spin_op operator-(spin_op op, double coeff) {
  return op - spin_op(op.n_qubits()) * coeff;
}

namespace spin {
spin_op i(const std::size_t idx) { return spin_op(pauli::I, idx); }
spin_op x(const std::size_t idx) { return spin_op(pauli::X, idx); }
spin_op y(const std::size_t idx) { return spin_op(pauli::Y, idx); }
spin_op z(const std::size_t idx) { return spin_op(pauli::Z, idx); }
} // namespace spin

std::vector<double> spin_op::getDataRepresentation() {
  std::vector<double> dataVec;
  for (auto &[term, coeff] : terms) {
    auto nq = terms.size() / 2;
    for (std::size_t i = 0; i < nq; i++) {
      if (term[i] && term[i + nq]) {
        dataVec.push_back(3.);
      } else if (term[i]) {
        dataVec.push_back(1.);
      } else if (term[i + nq]) {
        dataVec.push_back(2.);
      } else {
        dataVec.push_back(0.);
      }
    }
    dataVec.push_back(coeff.real());
    dataVec.push_back(coeff.imag());
  }
  dataVec.push_back(n_terms());
  return dataVec;
}

spin_op binary_spin_op_reader::read(const std::string &data_filename) {
  std::ifstream input(data_filename, std::ios::binary);
  if (input.fail())
    throw std::runtime_error(data_filename + " does not exist.");

  input.seekg(0, std::ios_base::end);
  std::size_t size = input.tellg();
  input.seekg(0, std::ios_base::beg);
  std::vector<double> input_vec(size / sizeof(double));
  input.read((char *)&input_vec[0], size);
  auto n_terms = (int)input_vec.back();
  auto nQubits = (input_vec.size() - 2 * n_terms) / n_terms;
  spin_op s(input_vec, nQubits);
  return s;
}
} // namespace cudaq
