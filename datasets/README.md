# 数据集导入说明

当前基础版本支持从 Qt 界面导入 UTF-8 编码的 CSV 文件，导入目标是本机 SQLite。
外部网站下载的数据通常需要先整理列名，不能把任意 CSV 原样塞进数据库。

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

必需列：

- `name`：食谱名称
- `total_calories`：非负热量值，单位 kcal
- `meal_type`：`breakfast`、`lunch`、`dinner`、`snack`，也接受早餐、午餐、晚餐、加餐

可选列：

- `id`：食谱编号；留空时自动生成稳定编号
- `ingredients`：多个食材用 `|` 分隔；单项格式为 `名称:数量:单位`
- `nutrition_tags`：多个标签用 `|`、英文逗号或中文逗号分隔

食材列也支持 JSON 数组，例如：

```text
[{"name":"鸡胸肉","amount":150,"unit":"g"}]
```

## 导入行为

- 相同编号再次导入时更新原记录，不会无限生成重复数据。
- 单行格式错误时跳过该行，其他正确行仍会保存。
- 导入结束后界面显示有效行数、SQLite 写入数和错误摘要。
- 原始 CSV 文件不会被修改。
