# 数据集导入说明

当前版本支持从 Qt 界面导入 UTF-8 编码的 CSV 文件，导入目标是本机 SQLite。
食谱导入器既支持项目模板，也支持 Food.com `recipes.csv` 的主要原始字段。

## 运动数据

参考 `templates/exercises_template.csv`。

必需列：

- `name`：运动名称
- `met_value`：大于 0 的 MET 值

可选列：

- `id`：运动编号；留空时自动生成稳定编号
- `category`：`aerobic`、`strength`、`flexibility`、`balance`、`other`，也接受有氧、力量、柔韧、平衡、其他
- `description`：运动说明

常见别名也可识别，例如 `activity`、`met`、`mets`、`type`。

## 预处理 2024 Adult Compendium

官方文件可先用 Excel “另存为” UTF-8 CSV。项目会构建
`compendium_preprocessor` C++ 命令行工具，可直接识别：

- `Major Heading`
- `Activity Code`
- `MET Value`
- `Activity Description`

在项目根目录执行：

```cmd
build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\compendium_preprocessor.exe ^
  --input "D:\datasets\2024-adult-compendium.csv" ^
  --output "datasets\builtin\exercises.csv" ^
  --max-per-category 150
```

工具会自动：

- 将 Compendium 大类和活动描述映射为有氧、力量、柔韧、平衡；
- 排除静坐、职业、自理等不适合作为运动处方的类别；
- 默认保留 MET 2.0–18.0 的记录；
- 在低、中、高三种强度中轮流选取，避免数据只剩一种强度；
- 使用 `CPA_活动编码` 作为稳定编号，并按编号和“名称 + MET”去重。

输出文件改变后需要重新构建主程序，使 Qt 资源包含新数据。

## 食谱数据

参考 `templates/recipes_template.csv`。

项目模板的必需信息：

- `name`：食谱名称
- `total_calories`：非负热量值，单位 kcal
- 餐别来源：使用 `meal_type`，或者使用 `RecipeCategory`/`Keywords` 自动推断
- 食材来源：使用 `ingredients`，或者使用 Food.com 的 `RecipeIngredientParts`

可选列：

- `id`：食谱编号；留空时自动生成稳定编号
- `ingredients`：多个食材用 `|` 分隔；单项格式为 `名称:数量:单位`
- `nutrition_tags`：多个标签用 `|`、英文逗号或中文逗号分隔
- `servings`：食谱份数，缺失或无效时默认为 1
- `protein_g`、`carbohydrate_g`、`fat_g`、`saturated_fat_g`、`fiber_g`、`sugar_g`
- `sodium_mg`、`cholesterol_mg`

食材列也支持 JSON 数组，例如：

```text
[{"name":"鸡胸肉","amount":150,"unit":"g"}]
```

## Food.com 字段映射

以下原始列可直接识别：

- `RecipeId`、`Name`、`Calories`、`RecipeServings`
- `ProteinContent`、`CarbohydrateContent`、`FatContent`
- `SaturatedFatContent`、`FiberContent`、`SugarContent`
- `SodiumContent`、`CholesterolContent`
- `RecipeCategory`、`Keywords`
- `RecipeIngredientParts`、`RecipeIngredientQuantities`

`Keywords`、配料名称和配料数量支持 Food.com 使用的 R 向量文本，
例如 `c("breakfast", "high-protein")`。餐别无法可靠推断或食材为空的记录会被跳过。

## 导入行为

- 相同编号再次导入时更新原记录，不会无限生成重复数据。
- 单行格式错误时跳过该行，其他正确行仍会保存。
- 支持英文双引号、字段中的逗号、转义双引号和引号字段中的换行。
- 为避免大型数据集占用过多内存，错误详情最多保留前 100 条。
- 导入结束后界面显示有效行数、SQLite 写入数和错误摘要。
- 原始 CSV 文件不会被修改。

## 预处理 Food.com 完整数据集

项目会构建一个名为 `foodcom_preprocessor` 的 C++ 命令行工具。下载原始
`recipes.csv` 后，在项目根目录执行：

```cmd
build\Desktop_Qt_6_11_2_MinGW_64_bit_Debug\foodcom_preprocessor.exe ^
  --input "D:\datasets\recipes.csv" ^
  --output "datasets\builtin\recipes.csv" ^
  --max-per-meal 250
```

工具会自动：

- 解析 Food.com 原始字段和餐别；
- 排除热量、食材数量或营养数据不完整的记录；
- 按蛋白质、纤维、目标热量、饱和脂肪和钠进行基础质量排序；
- 按名称和编号去重；
- 每种餐别保留指定数量；
- 输出项目统一格式的 UTF-8 CSV。

输出文件发生变化后需要重新构建主程序，使 Qt 资源包含新数据。

## 程序首次启动自动导入

`datasets/builtin/exercises.csv` 和 `datasets/builtin/recipes.csv` 都会编译进程序资源。程序启动时计算文件的
SHA-256 指纹，并在 SQLite 的 `dataset_imports` 表中记录版本：

- 第一次启动：自动新增或更新内置运动与食谱；
- 文件没有变化：跳过导入；
- 重新预处理并构建后：检测到指纹变化，再次升级导入；
- 任意写入失败：事务回滚，不留下半套数据。

## 直接批量写入 SQLite

完整数据集不必编译进 Qt 资源。先用上述预处理工具生成标准
CSV，关闭正在运行的主程序后执行：

```cmd
build-codex\dataset_importer.exe ^
  --recipes "D:\datasets\processed\recipes.csv" ^
  --exercises "D:\datasets\processed\exercises.csv"
```

省略 `--database` 时，工具使用与 Qt 主程序相同的 SQLite 文件。
也可使用 `--database "D:\data\weight_agent.db"` 指定其他数据库。

导入器使用事务和预编译 UPSERT：

- 相同 ID 自动更新，新 ID 自动新增；
- 文件指纹没有变化时跳过重复导入；
- 任意一条写入失败时回滚整批数据；
- 不会删除用户、历史计划或其他已有记录。

## 大范围候选检索

数据库将蛋白质、碳水、脂肪、纤维、糖、钠等营养值同步保存为
可索引的数值列，不再只保存在 JSON 中。生成计划时：

- 食谱按餐别、目标热量、营养范围和排除 ID 在 SQLite 中查询；
- 运动按类别、MET 范围、目标 MET 和排除 ID 查询；
- 每个餐别取最接近目标的 12 条，运动取 24 条；
- 组合算法只处理精选候选，避免数据量增大后枚举爆炸。
