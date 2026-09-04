# GrhSIM IR Pass System

本文定义 GrhSIM IR 的 pass 模型和 Python 编排接口。Pass 可以由 C++ 或 Python 实现，但都通过
同一套 Python API 组合和执行。

## 1. 边界

Pass manager 构建或修改 [GrhSIM IR Overview](../overview.md) 中定义的 `GrhSimIR`：

```text
GrhSimIR
  model: SimModel
  backends: BackendId -> B_<backend>
```

`PassManager.build(model)` 从单独的 `SimModel` 构建 `GrhSimIR`，但只有至少产生一个合法
`B_<backend>` 后才能返回。`PassManager.run(ir)` 的输入和输出都是已经包含至少一个后端映射的
合法 `GrhSimIR`。

Pass 之间只传递修改后的 `SimModel` 或完整的 `B_<backend>`。Pass 内部的临时计算在该 pass
结束时消失，不能通过全局变量、未命名 session 项或其他隐藏状态传给下一个 pass。

Python 负责：

- 选择 pass 及其参数；
- 决定 pass 的先后顺序；
- 使用普通 Python 条件、循环和函数组合 pipeline。

Pass 系统不另外定义控制流语言。

## 2. Pass 种类

### 2.1 `ModelPass`

```text
ModelPass.run(model: SimModel) -> PassResult<SimModel>
```

`ModelPass` 可以修改 `I`、`O`、`S`、`F`、`G` 或 `Init`，返回一个完整的新 `SimModel`。方言转换、
图重写和模型规范化都属于这一类。

一旦 `ModelPass` 返回 `changed = true`，已有的全部 `B_<backend>` 都失效。失效状态只存在于
当前 `PassManager` 执行期间，不是 `GrhSimIR` 的字段；在对应映射重新生成前，pass manager
不能返回结果。

### 2.2 `BackendBuildPass`

```text
BackendBuildPass
  backend: BackendId
  run(model: SimModel) -> PassResult<B_<backend>>
```

`BackendBuildPass` 根据一个完整 `SimModel` 创建该后端的完整映射。例如，CPU backend 的 build
pass 必须一次返回同时包含 `DataLayout`、`PartitionTree` 和 `SchedulePlan` 的合法 `B_cpu`，
不能把未完成的后端映射留给下一 pass。

### 2.3 `BackendPass`

```text
BackendPass
  backend: BackendId
  run(model: SimModel, mapping: B_<backend>) -> PassResult<B_<backend>>
```

`BackendPass` 修改一个已经存在的后端映射。它可以重新规划映射的一个或多个分量，但返回值
仍必须是完整的 `B_<backend>`。它不能修改 `SimModel`，也不能修改其他后端的映射。

### 2.4 `PassResult`

```text
PassResult<T>
  value: T
  changed: Boolean
```

`value` 是 pass 的完整替代输出。Pass 不得原地修改输入对象；`changed = false` 表示输出与输入
相同，此时可以直接返回原对象。`PassResult` 只是 pass API 的返回值，不是 GrhSIM IR 的分量。

## 3. Python 接口

Python 包 `wolvrix.grhsim_ir` 提供以下接口：

`BackendMapping` 是所有后端专属 `B_<backend>` 在 Python 中的公共基类；具体字段仍由对应
backend 定义。

```python
class ModelPass(Protocol):
    name: str
    def run(self, model: SimModel) -> PassResult[SimModel]: ...

class BackendBuildPass(Protocol):
    name: str
    backend: str
    def run(self, model: SimModel) -> PassResult[BackendMapping]: ...

class BackendPass(Protocol):
    name: str
    backend: str
    def run(
        self,
        model: SimModel,
        mapping: BackendMapping,
    ) -> PassResult[BackendMapping]: ...

Pass = ModelPass | BackendBuildPass | BackendPass

class PassManager:
    def add(self, pass_: Pass) -> None: ...
    def build(self, model: SimModel) -> GrhSimIR: ...
    def run(self, ir: GrhSimIR) -> GrhSimIR: ...
```

Python 实现只要满足对应 protocol 就能加入 `PassManager`。C++ pass 通过 Python binding 暴露为
满足同一 protocol 的对象，因此 pipeline 不区分 pass 的实现语言。

典型编排形式为：

```python
pm = PassManager()
pm.add(model_pass)
pm.add(cpu_build_pass)
pm.add(cpu_backend_pass)

ir = pm.build(model)
```

`model_pass`、`cpu_build_pass` 和 `cpu_backend_pass` 都是已经构造好的 pass 对象。Python 可以在
调用 `add` 前根据目标机器、配置或模型内容选择对象，不需要把控制流编码进 pass manager。

## 4. Pass 注册

需要按名称创建的 pass 通过统一 registry 注册：

```python
class PassRegistry:
    def register(self, name: str, factory: Callable[..., Pass]) -> None: ...
    def create(self, name: str, **options) -> Pass: ...
    def names(self) -> list[str]: ...
```

Pass 名称必须全局唯一，并使用 `<owner>.<name>` 形式。方言提供的 model pass 使用方言名作为
owner，后端 build/backend pass 使用 backend 名作为 owner。Native binding 和 Python 模块都向
同一个 registry 注册 factory；factory 负责检查自己的 Python 参数。

Registry 只负责从名称和参数创建 pass，不保存 pass 的执行结果。

## 5. 执行规则

`PassManager.build` 和 `PassManager.run` 都按 `add` 的顺序执行，并遵守以下规则：

1. `build` 开始前验证输入 `SimModel`；`run` 开始前验证输入 `GrhSimIR`，包括其非空
   `backends` 和每个 `B_<backend>`。
2. `ModelPass` 返回后先验证新的 `SimModel`；只有验证成功才替换旧模型。若 `changed = true`，
   当前执行中的全部旧后端映射立即失效。
3. `BackendBuildPass` 返回后验证其完整映射，成功后写入对应 `BackendId`。同一 ID 已有映射时，
   新映射替换旧映射，并恢复该 ID 的有效状态。
4. `BackendPass` 只能在对应映射存在且有效时运行；返回后验证完整映射，成功后替换旧映射。
5. 任一 pass 报错或验证失败时立即停止，失败输出不写入 `GrhSimIR`，后续 pass 不执行。
6. 全部 pass 完成后，只有 `backends` 非空且其中没有失效映射时才能返回。否则本次执行失败，
   不产生部分构建或缺少后端映射的 `GrhSimIR`。

Pass manager 只根据 pass 种类执行上述固定行为。Pass 的名称、注册顺序或 Python 对象类型不能
绕过模型和后端之间的修改边界。

## 6. 验证与失效

`SimModel` 的验证规则由 overview 和当前已加载方言共同定义；`B_<backend>` 的验证规则由
overview 和对应 backend 共同定义。验证是 pass manager 的固定步骤，不作为可选 pass 插入
pipeline。

模型修改和后端映射之间只有一条失效规则：任何 `ModelPass` 改变 `SimModel`，所有现存
`B_<backend>` 都失效。每个映射必须由对应 `BackendBuildPass` 重新生成；`BackendPass` 只能处理
已经有效的映射。pass manager 绝不返回 `backends` 为空或包含失效映射的 `GrhSimIR`。

## 7. 文档

- [GrhSIM IR Overview](../overview.md)
- [Core Dialect](../dialects/core.md)
- [CPU Backend](../backends/cpu.md)
