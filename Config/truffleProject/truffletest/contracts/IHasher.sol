// SPDX-License-Identifier: MIT
pragma solidity ^0.6.12;

interface IHasher {
  function MiMCSponge(uint256 in_xL, uint256 in_xR) external pure returns (uint256 xL, uint256 xR);
}

