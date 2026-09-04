# GrhSIM CPU Backend

本文定义 CPU backend 对 GrhSIM `SimModel` 的映射 `B_cpu`。[GrhSIM IR Overview](../overview.md)
定义通用模型和后端边界；本文中的三个分量仅属于 CPU backend，不是其他 backend 必须采用的
结构。

## 1. `B_cpu`

```text
B_cpu
  data_layout: DataLayout
  partition_tree: PartitionTree
  schedule_plan: SchedulePlan
```

- `DataLayout` 为 I/O/S 对象和 graph value 选择 CPU 物理表示与存储位置；
- `PartitionTree` 把 `G` 的 op 组织为层次化分区；
- `SchedulePlan` 把分区映射到 NUMA node 和 CPU core，并规定执行顺序与跨 core 依赖。

三个分量共同映射同一个 `SimModel`，但不改变其 op、value、对象或执行语义。这里的排列只是
数据结构的组成，不规定三个分量的生成顺序。

## 2. `DataLayout`

```text
DataLayout
  object_layouts: ObjectRef -> CpuDataLayout
  value_layouts: ValueId -> CpuDataLayout

CpuDataLayout
  cpu_type: CpuTypeRef
  size_bytes: Integer
  alignment_bytes: Integer
  storage: CpuStorage

CpuStorage
  kind: object | partition_local | boundary
  owner: PartitionId?
```

`object_layouts` 覆盖 `I`、`O` 和 `S` 中的每个对象，`value_layouts` 覆盖 `G` 中的每个 value。
`F` 中的外部函数声明不是数据对象，不在 `object_layouts` 的覆盖范围；外部函数如何绑定到具体
实现由 CPU backend 自行定义。`CpuTypeRef` 引用 CPU backend 支持的一种物理类型；该类型定义
如何表示原对象或 value 的方言语义类型。例如，同一个 `core.logic` 类型可以根据位宽和 2-state/4-state 语义映射为机器整数、
机器字数组或分别保存 value/known bits 的结构。

`CpuTypeRef` 与 `TypeRef` 不同：`TypeRef` 属于 `SimModel` 并定义语义，`CpuTypeRef` 只属于
`B_cpu` 并定义 CPU 物理表示。CPU 专属方言中的类型仍然是 `TypeRef`，不属于 `DataLayout`。

`size_bytes` 和 `alignment_bytes` 给出该表示在 CPU 内存中的大小和对齐，两者必须与
`cpu_type` 一致。`storage` 的三种取值分别表示：

- `object`：I/O/S 对象占用的长期存储，`owner` 为空；
- `partition_local`：只在一个分区内部使用的 value，`owner` 指向该分区；
- `boundary`：跨分区传递的 value，`owner` 指向产生该 value 的分区。

一个 value 是否跨分区，由 `G` 中的定义/使用关系和 `PartitionTree` 共同确定。`DataLayout`
必须据此选择 `partition_local` 或 `boundary`，不能改变 value 的生产者或使用者。

`DataLayout` 不定义初始化语义，也不保存初值。`Init` 始终是 `SimModel` 的分量；CPU 实现只按
`DataLayout` 指定的物理表示，在实例创建时对每个 `InitSpec` 求值并把结果写入对应 `S` 对象。

## 3. `PartitionTree`

```text
PartitionTree
  root: PartitionId
  partitions: PartitionId -> Partition

Partition
  parent: PartitionId?
  children: PartitionId[]
  ops: OpId[]
```

`PartitionTree` 满足以下约束：

- `root` 是唯一没有 `parent` 的分区；
- 每个非根分区恰好有一个父分区，父子引用必须互相一致且不能形成环；
- 只有叶分区直接包含 `ops`，每个 `G` 中的 op 恰好属于一个叶分区；
- 一个非叶分区覆盖其全部后代叶分区中的 op，根分区覆盖整个 `G`。

CPU backend 可以把叶分区作为 node，把由若干子分区组成的非叶分区作为 supernode。node 和
supernode 是 `PartitionTree` 中的层次关系，不是新的 graph op。

value 仍然是 `G` 中的边。跨分区 value 可以由其生产 op 和使用 op 所属的叶分区推导，不在
`PartitionTree` 中重复保存。

## 4. `SchedulePlan`

```text
SchedulePlan
  numa_nodes: NumaNodeSchedule[]

NumaNodeSchedule
  numa_node: NumaNodeId
  cores: CpuCoreSchedule[]

CpuCoreSchedule
  core: CpuCoreId
  tasks: ScheduledTask[]

ScheduledTask
  id: TaskId
  partition: PartitionId
  waits_for: TaskId[]
```

`NumaNodeId` 标识目标 CPU 的一个 NUMA 内存域，`CpuCoreId` 标识该内存域中的一个 CPU 执行核。
二者都是 CPU backend 的目标资源 ID；这里的 CPU core 与 GrhSIM `core` 方言无关。每个
`CpuCoreId` 只能属于一个 `NumaNodeSchedule`，task 在执行期间不能迁移到其他 core。

`tasks` 的数组顺序就是同一 CPU core 上的执行顺序。不同 core 各自执行自己的 task 序列，
因此可以并行；一个 task 必须等其全部 `waits_for` task 完成后才能开始。`waits_for` 用于表达
跨 core 或跨 NUMA node 的必要顺序，同一 core 内的顺序不需要重复写入。

一个 task 执行 `partition` 覆盖的全部 op。被 task 引用的分区必须互不包含，并共同覆盖根分区：
每个叶分区恰好被自身或它的一个祖先分区所覆盖。这样既可以把 node 作为执行单元，也可以把
包含多个 node 的 supernode 整体分配给一个 CPU core，而不会重复执行 op。

task 从所在的 `CpuCoreSchedule` 继承 CPU core 和 NUMA node 归属。`DataLayout` 可以根据该归属
区分同 core、同 NUMA node 和跨 NUMA node 的 value，但 value 的生产者和使用者仍由 `G`
决定。

所有 task 序列与 `waits_for` 合成的顺序必须无环，并覆盖 `G` 中跨 task value 的生产者到
使用者关系。该顺序还必须满足方言规定的对象访问约束和可观察 op 相对顺序。

## 5. 验证

`B_cpu` 必须满足：

- `DataLayout` 完整覆盖当前 `SimModel` 的 I/O/S/value，且物理类型、大小、对齐和存储种类
  相互一致；
- `partition_local` 和 `boundary` 引用的分区存在，且与 `G` 的 value 定义/使用关系一致；
- `PartitionTree` 是一棵覆盖全部 op 且不重复归属 op 的树；
- NUMA node 和 CPU core ID 唯一，每个 CPU core 只属于一个 NUMA node；
- task ID 唯一，`waits_for` 只引用当前计划中的 task；task 所引用的分区互不重叠并完整覆盖
  `PartitionTree`；
- core 内序列与 `waits_for` 共同构成无环顺序，并满足 graph 数据流、方言对象访问语义和
  可观察 op 相对顺序；
- `B_cpu` 不增加、删除或改写 `SimModel` 的 `Init` 条目。

`SimModel` 或其所引用的方言版本发生变化后，原有 `B_cpu` 失效，必须重新生成并验证。

## 6. 参考

- [GrhSIM IR Overview](../overview.md)
- [Core Dialect](../dialects/core.md)
- [Pass System](../passes/overview.md)
