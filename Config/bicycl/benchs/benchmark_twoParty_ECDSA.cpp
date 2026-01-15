/*
 * BICYCL Implements CryptographY in CLass groups
 * Copyright (C) 2024  Cyril Bouvier <cyril.bouvier@lirmm.fr>
 *                     Guilhem Castagnos <guilhem.castagnos@math.u-bordeaux.fr>
 *                     Laurent Imbert <laurent.imbert@lirmm.fr>
 *                     Fabien Laguillaumie <fabien.laguillaumie@lirmm.fr>
 *                     Quentin Combal <quentin.combal@lirmm.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <sstream>
#include <string>

#include "bicycl.hpp"
#include "internals.hpp"

using std::string;

using namespace BICYCL;
using BICYCL::Bench::ms;
using BICYCL::Bench::us;

using Signature = TwoPartyECDSA::Signature;

/* */
void two_party_ECDSA_benchs (const SecLevel & seclevel,
                                  RandGen & randgen,
                                  const string & pre,
                                  unsigned int niter)
{
  std::stringstream desc;
  desc << pre;

  //******************************** Init **********************************//
  auto init = [&seclevel, &randgen] () {
    TwoPartyECDSA C(seclevel, randgen);
  };
  Bench::one_function<ms, ms>(init, 10u, "Init", desc.str());

  TwoPartyECDSA C(seclevel, randgen);
  std::cout << std::endl;

  //******************************** Keygen **********************************//
  TwoPartyECDSA::Player1 P1{C};
  TwoPartyECDSA::Player2 P2{C};

  //************ KeygenPart1 ************//
  auto keygen1 = [&C, &P1] () {
    P1.KeygenPart1(C);
  };
  Bench::one_function<ms, ms>(keygen1, niter, "KeygenPart1", desc.str());

  /* Communication: P1 com-proof */
  size_t keygen_R1_comm_size_bytes = P1.commit().size();


  //************ KeygenPart2 ************//
  auto keygen2 = [&C, &P1, &P2] () {
    // P1 --- com-proof(Q1) ---> P2
    P2.KeygenPart2(C, P1.commit());
  };
  Bench::one_function<ms, ms>(keygen2, niter, "KeygenPart2", desc.str());

  /* Communication: Q2 + zk proof */
  OpenSSL::BN xcoord, ycoord;
  C.ec_group().coords_of_point(xcoord, ycoord, P2.Q2());
  size_t keygen_R2_comm_size_bytes = xcoord.num_bytes() + ycoord.num_bytes();

  C.ec_group().coords_of_point(xcoord, ycoord, P2.zk_proof().R());
  keygen_R2_comm_size_bytes += xcoord.num_bytes() + ycoord.num_bytes();
  keygen_R2_comm_size_bytes += P2.zk_proof().z().num_bytes();


  //************ KeygenPart3 ************//
  auto keygen3 = [&C, &randgen, &P1, &P2] () {
    // P1 < --- (Q2, zk-proof(Q2)) --- P2
    P1.KeygenPart3(C, randgen, P2.Q2(), P2.zk_proof());
  };
  Bench::one_function<ms, ms>(keygen3, niter, "KeygenPart3", desc.str());

  /* Communication: Q1, x1-proof, commit-secret, Ckey, pkcl, ckey-proof*/
  // Q1
  C.ec_group().coords_of_point(xcoord, ycoord, P1.Q1());
  size_t keygen_R3_comm_size_bytes = xcoord.num_bytes() + ycoord.num_bytes();
  // x1-proof
  C.ec_group().coords_of_point(xcoord, ycoord, P1.zk_com_proof().R());
  keygen_R3_comm_size_bytes += xcoord.num_bytes() + ycoord.num_bytes();
  keygen_R3_comm_size_bytes += P1.zk_com_proof().z().num_bytes();

  // commit-secret
  keygen_R3_comm_size_bytes += P1.commit_secret().size();
  // Ckey
  keygen_R3_comm_size_bytes += (P1.Ckey().c1().compressed_repr().nbits()+7u)/8u;
  keygen_R3_comm_size_bytes += (P1.Ckey().c2().compressed_repr().nbits()+7u)/8u;
  // pkcl
  keygen_R3_comm_size_bytes += (P1.pkcl().elt().compressed_repr().nbits()+7u)/8u;
  // ckey-proof
  keygen_R3_comm_size_bytes += (P1.proof_ckey().u1().nbits()+7u)/8u;
  keygen_R3_comm_size_bytes += (P1.proof_ckey().u2().nbits()+7u)/8u;
  keygen_R3_comm_size_bytes += (P1.proof_ckey().chl().nbits()+7u)/8u;
  C.ec_group().coords_of_point(xcoord, ycoord, P1.proof_ckey().R());
  keygen_R3_comm_size_bytes += xcoord.num_bytes() + ycoord.num_bytes();

  //************ KeygenPart4 ************//
  auto keygen4 = [&C, &P1, &P2] () {
    // P1 --- (Q1, pk, Ckey, decomp-proof(Q1)) ---> P2
    P2.KeygenPart4(C,
                   P1.Q1(),
                   P1.Ckey(),
                   P1.pkcl(),
                   P1.commit_secret(),
                   P1.zk_com_proof(),
                   P1.proof_ckey());
  };
  Bench::one_function<ms, ms>(keygen4, niter, "KeygenPart4", desc.str());

  std::cout << std::endl;

  //******************************** Sign **********************************//

  //************ SignPart1 ************//
  auto sign1 = [&C, &P1] () {
    P1.SignPart1(C);
  };
  Bench::one_function<ms, ms>(sign1, niter, "SignPart1", desc.str());

  /* Communication: P1 com-proof */
  size_t sign_R1_comm_size_bytes = P1.commit().size();

  //************ SignPart2 ************//
  auto sign2 = [&C, &P1, &P2] () {
    P2.SignPart2(C, P1.commit());
  };
  Bench::one_function<ms, ms>(sign2, niter, "SignPart2", desc.str());

  /* Communication: R2 + zk proof */
  C.ec_group().coords_of_point(xcoord, ycoord, P2.R2());
  size_t sign_R2_comm_size_bytes = xcoord.num_bytes() + ycoord.num_bytes();

  C.ec_group().coords_of_point(xcoord, ycoord, P2.zk_proof().R());
  sign_R2_comm_size_bytes += xcoord.num_bytes() + ycoord.num_bytes();
  sign_R2_comm_size_bytes += P2.zk_proof().z().num_bytes();

  //************ SignPart3 ************//
  auto sign3 = [&C, &P1, &P2] () {
    // P1 < --- R2 --- P2
    P1.SignPart3(C, P2.R2(), P2.zk_proof());
  };
  Bench::one_function<ms, ms>(sign3, niter, "SignPart3", desc.str());

  /* Communication: R1, k1-proof, commit-secret */
  // R1
  C.ec_group().coords_of_point(xcoord, ycoord, P1.Q1());
  size_t sign_R3_comm_size_bytes = xcoord.num_bytes() + ycoord.num_bytes();
  // k1-proof
  C.ec_group().coords_of_point(xcoord, ycoord, P1.zk_com_proof().R());
  sign_R3_comm_size_bytes += xcoord.num_bytes() + ycoord.num_bytes();
  sign_R3_comm_size_bytes += P1.zk_com_proof().z().num_bytes();
  // commit-secret
  sign_R3_comm_size_bytes += P1.commit_secret().size();

  //************ SignPart4 ************//
  const TwoPartyECDSA::Message & message = random_message();

  auto sign4 = [&C, &randgen, &P1, &P2, &message] () {
    // P1 --- R1 --- > P2
    P2.SignPart4(
        C, randgen, message, P1.R1(), P1.commit_secret(), P1.zk_com_proof());
  };
  Bench::one_function<ms, ms>(sign4, niter, "SignPart4", desc.str());

  /* Communication: C3 */
  size_t sign_R4_comm_size_bytes
      = (P2.C3().c1().compressed_repr().nbits() + 7u
         + P2.C3().c2().compressed_repr().nbits() + 7u);
  sign_R4_comm_size_bytes /= 8u;

  //************ SignPart5 ************//
  auto sign5 = [&C, &P1, &P2] () {
    // P1 < --- C3 --- P2
    Signature signature = P1.SignPart5(C, P2.C3());
  };
  Bench::one_function<ms, ms>(sign5, niter, "SignPart5", desc.str());

/************************** COMMUNICATION COSTS ***************************/

  std::cout << std::fixed;
  std::cout << "\n    Phase | Sub phase | Player |"
            << "    Size   \n";

  std::cout << "   Keygen |  Round 1  |   P1   |"
            << std::setprecision(2) << std::setw(6)
            << keygen_R1_comm_size_bytes << " bytes\n";
  std::cout << "          |  Round 2  |   P2   |"
            << std::setprecision(2) << std::setw(6)
            << keygen_R2_comm_size_bytes << " bytes\n";
  std::cout << "          |  Round 3  |   P1   |"
            << std::setprecision(2) << std::setw(6)
            << keygen_R3_comm_size_bytes << " bytes\n";

  std::cout << "     Sign |  Round 1  |   P1   |"
            << std::setprecision(2) << std::setw(6)
            << sign_R1_comm_size_bytes << " bytes\n";
  std::cout << "          |  Round 2  |   P2   |"
            << std::setprecision(2) << std::setw(6)
            << sign_R2_comm_size_bytes << " bytes\n";
  std::cout << "          |  Round 3  |   P1   |"
            << std::setprecision(2) << std::setw(6)
            << sign_R3_comm_size_bytes << " bytes\n";
  std::cout << "          |  Round 4  |   P2   |"
            << std::setprecision(2) << std::setw(6)
            << sign_R4_comm_size_bytes << " bytes\n";

  std::cout << std::flush;
}

/* */
int main (int argc, char * argv[])
{
  RandGen randgen;
  randseed_from_argv(randgen, argc, argv);

  Bench::print_thread_info();

  // Override OpenSSL rng with randgen
  Test::OverrideOpenSSLRand::WithRandGen tmp_override(randgen);

  std::cout << "security level  |    operation    |   time   "
            << "| time per op. " << std::endl;

  for (const SecLevel seclevel : SecLevel::All())
  {
    unsigned int niter = (seclevel.nbits() < 192u) ? 100u : 10u;

    std::stringstream desc;
    desc << "  " << seclevel << " bits     ";

    two_party_ECDSA_benchs(seclevel, randgen, desc.str(), niter);

    std::cout
        << "------------------------------------------------------------------"
        << std::endl;
  }

  return EXIT_SUCCESS;
}
