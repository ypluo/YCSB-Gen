### YCSB-Gen

This is a YCSB workload generator. It is modified from project [YCSB-C](https://github.com/basicthinker/YCSB-C). 

We strip functionalities other than generating dataset and query workload from default key space. Besides, we add support for user-defined key space, which is very helpful in testing dataset-sensitive indices or key value stores.

#### Usage

- To build YCSB-Gen on ubuntu, for example:

  ```shell
  mkdir build
  cd build & make
  ```

- To generate a dataset and workload file from default key space.

  ```shell
  ./ycsbc -P workloads/workloada.spec
  ```

  This command will generate two files, `dataset.dat` and `query.dat`. The `dataset.dat` contains insert queries to load initial data against your DB, and the latter contains workloads to benchmark your DB.

- To generate a dataset and workload file from given key space. You should provide a key set file that contains sufficient lines of keys that you want to draw from.

  ```shell
  ./ycsbc -P workloads/workloada.spec -F <your keyset file>
  ```
