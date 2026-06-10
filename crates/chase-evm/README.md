# chase-evm

EVM fork for the [CHASE](https://github.com/xpk1998/optme) blockchain execution system.

Forked from [rust-evm](https://github.com/sorpaas/rust-evm) with CHASE-specific extensions (e.g. `MultiversionView` for shared-multiversion data structures). Previously published as `optme-evm` (`Dong-Hyeon-Yu/optme-evm` @ `d81889d`).

## Workspace usage

Vendored under `crates/chase-evm` and referenced from the root workspace:

```toml
evm = { path = "crates/chase-evm" }
```

To publish as a standalone repository, copy this directory to `xpk1998/chase-evm` and switch the workspace dependency back to a git pin.
