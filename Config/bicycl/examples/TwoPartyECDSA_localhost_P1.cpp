// Client side implementation of UDP client-server model

#include "bicycl.hpp"
#include "network_helper.hpp"

#define PORTOUT 8080
#define PORTIN 8080

/* Aliases */
using namespace BICYCL;
using BICYCL::OpenSSL::ECPoint;

/* Global vars */
int udp_socket_fd;

/* Main */
int main ()
{
  std::cout << "TwoParty ECDSA P1\n" << std::endl;

  //*********************** UDP COM init *************************//
  struct sockaddr_in P1_addr, P2_addr;

  // Creating socket file descriptor
  if ((udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Filling P1 information
  P1_addr.sin_family = AF_INET;
  P1_addr.sin_addr.s_addr = 0x0100007f; // 127.0.0.1
  P1_addr.sin_port = htons(PORTIN);

  // Filling P2 information
  P2_addr.sin_family = AF_INET;
  P2_addr.sin_addr.s_addr = 0x0200007f; // 127.0.0.2
  P2_addr.sin_port = htons(PORTOUT);

  // Bind the socket with the server address
  if (bind(udp_socket_fd, (const struct sockaddr *)&P1_addr, sizeof(P1_addr)) < 0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  std::cout << "UDP socket open\n" << std::endl;


  //*********************** 2P protocol init *************************//

  /* A random generator. */
  RandGen randgen_public, randgen_private;

  /* For reproducibility, the seed for publc rng is fixed. */
  Mpz seed("0xCAFEDECAF0CACC1ABABAC001DED1CACEEFF1CACE");
  randgen_public.set_seed(seed);

  /* Send seed to Player 2*/
  std::cout << "Init: Sending seed..." << std::flush;
  send(P2_addr, seed);
  std::cout << " OK" << std::endl;

  /* The desired security level. */
  SecLevel seclevel(128);

  /* Hash function with the desired soundness */
  OpenSSL::HashAlgo H(seclevel);

  /* Two Party ECDSA:
   * Public parameters of CL and EC are generated according to the given
   * security level.
   */
  std::cout << "Init: init CL..." << std::flush;
  TwoPartyECDSA context(seclevel, randgen_public);
  std::cout << " OK\n" << std::endl;

  /* Print some details about the cryptosystem CL. */
  // std::cout << "--- CL ---\n" << context.CL_HSMq() << std::endl;

  /* Print some details about the Elliptic Curve. */
  // std::cout << "--- Elliptic Curve ---\n" << context.ec_group() << std::endl;

  TwoPartyECDSA::Player1 P1(context);

  //*********************** KEYGEN *************************//
  /* Keygen Round 1 */
  std::cout << "Keygen Round 1: computing..." << std::flush;
  P1.KeygenPart1(context);
  std::cout << "  done" << std::endl;

  /* P1 --- com-proof(Q1) ---> P2 */
  std::cout << "Keygen Round 1: P1 --- com-proof(Q1) ---> P2" << std::endl;
  std::cout << "Keygen Round 1: sending..." << std::flush;
  const TwoPartyECDSA::CommitmentSecret & commit = P1.commit();
  send (P2_addr, commit.data(), commit.size());
  std::cout << " OK" << std::endl;

  /* Keygen Round 2 */
  std::cout << "Keygen Round 2: P1 < --- (Q2, zk-proof(Q2)) --- P2" << std::endl;
  std::cout << "Keygen Round 2: waiting for P2..." << std::flush;

  /* P1 < --- (Q2, zk-proof(Q2)) --- P2 */
  ECPoint Q2 = receive_ECPoint(P2_addr, context.ec_group());
  ECNIZKProof P2_zk_proof = receive_ECNIZKproof(P2_addr, context.ec_group());
  std::cout << " OK" << std::endl;

  /* Keygen round 3 */
  std::cout << "Keygen Round 3: computing..." << std::flush;
  P1.KeygenPart3(context, randgen_private, Q2, P2_zk_proof);
  std::cout << " OK" << std::endl;

  /* P1 --- (Q1, pk, Ckey, decomp-proof(Q1), proof_Ckey) ---> P2 */
  std::cout << "Keygen Round 3: P1 --- (Q1, pk, Ckey, decomp-proof(Q1)) ---> P2" << std::endl;
  std::cout << "Keygen Round 3: sending..." << std::flush;
  send(P2_addr, context.ec_group(), P1.Q1());
  send(P2_addr, P1.pkcl().elt(), true);
  send(P2_addr, P1.Ckey(), true);
  send(P2_addr, P1.commit_secret());
  send(P2_addr, context.ec_group(), P1.zk_com_proof());
  send(P2_addr, context.ec_group(), P1.proof_ckey());
  std::cout << " OK" << std::endl;

  std::cout << "\nKeygen done successfully for P1" << std::endl;

  //*********************** SIGN *************************//

  /* m is the message, it contains 16 random bytes */
  TwoPartyECDSA::Message m(16);
  OpenSSL::random_bytes (m.data(), m.size() * sizeof (uint8_t));

  /* Send message to P2 */
  send(P2_addr, m);

  std::cout << "\nmessage = " << std::hex;
  for (uint8_t byte : m) {
    std::cout << static_cast<unsigned int>(byte);
  }
  std::cout << '\n' << std::endl;

  /* Sign Round 1 */
  std::cout << "Sign Round 1: computing..." << std::flush;
  P1.SignPart1(context);
  std::cout << " OK" << std::endl;

  /* P1 --- com-proof(R1) ---> P2 */
  std::cout << "Sign Round 1: P1 --- com-proof(R1) ---> P2" << std::endl;
  std::cout << "Sign Round 1: sending..." << std::flush;
  send(P2_addr, P1.commit());
  std::cout << " OK" << std::endl;

  /* Sign Round 2 */
  /* P1 < --- (R2, zk-proof(R2)) --- P2 */
  std::cout << "Sign Round 2: P1 < --- (R2, zk-proof(R2)) --- P2" << std::endl;
  std::cout << "Sign Round 2: waiting for P2..." << std::flush;
  Q2 = receive_ECPoint(P2_addr, context.ec_group());
  P2_zk_proof = receive_ECNIZKproof(P2_addr, context.ec_group());
  std::cout << " OK" << std::endl;

  /* Sign Round 3 */
  std::cout << "Sign Round 3: computing..." << std::flush;
  P1.SignPart3(context, Q2, P2_zk_proof);
  std::cout << " OK" << std::endl;

  /* P1 --- (R1, decomp-proof(R1)) --- > P2 */
  std::cout << "Sign Round 3: P1 --- (R1, decomp-proof(R1)) ---> P2" << std::endl;
  std::cout << "Sign Round 3: sending..." << std::flush;
  send(P2_addr, context.ec_group(), P1.R1());
  send(P2_addr, P1.commit_secret());
  send(P2_addr, context.ec_group(), P1.zk_com_proof());
  std::cout << " OK" << std::endl;

  /* Sign round 4 */
  std::cout << "Sign Round 4: P1 < --- C3 --- P2" << std::endl;
  std::cout << "Sign Round 4: waiting for P2..." << std::flush;
  /* P1 < --- C3 --- P2 */
  TwoPartyECDSA::CLCipherText C3
      = receive_CLcipher(P2_addr, true, context.CL_HSMq().Delta());
  std::cout << " OK" << std::endl;

  /* Sign Round 5 */
  std::cout << "Sign Round 5: computing..." << std::flush;
  TwoPartyECDSA::Signature signature = P1.SignPart5(context, C3);
  std::cout << " OK" << std::endl;

  std::cout << "\nSignature done successfully for P2:\ns = " << signature << std::endl;

  bool verif_success = context.verify(signature, P1.public_key(), m);

  std::cout << "\nECDSA signature verification: "
            << (verif_success ? "OK" : "KO")
            << std::endl;

  close(udp_socket_fd);
  std::cout << "\nUDP socket closed\n" << std::endl;
  return 0;
}