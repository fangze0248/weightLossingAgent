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
