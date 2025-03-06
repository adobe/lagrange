# About Project Lagrange

Project Lagrange is an initiative to bring the power of robust geometry processing to Adobe products. It bridges cutting edge research works with cutting edge products. Project Lagrange is built on the following design principles:

### Modular design

Large features should be decomposed into smaller single functionality modules that are as decoupled as possible from each other. Modular design enables unit testing, prevents small changes from propagating widely in the code base, and makes adding new functionalities easy.

### Preconditions + guarantees

Algorithmic correctness should be rigorously enforced. This is achieved by clearly documenting and checking the precise precondition and the corresponding guarantees of each module. Algorithms relying on input-dependent parameter tuning should be avoided.

### Interface + compute engine

The interface of a functionality should be decoupled from the computation algorithms. This makes swapping out an algorithm with a better algorithm possible and ideally should not require changes in client codes.

### Large scale testing

Large scale, empirical testing on major functionalities should be carried out periodically to ensure their correctness and robustness. Let data speak for itself.

## Contributing

Contributions are welcomed! Read the [Contributing Guide](./.github/CONTRIBUTING.md) for more information.

## Licensing

This project is licensed under the Apache 2.0 License. See [LICENSE](LICENSE) for more information.
