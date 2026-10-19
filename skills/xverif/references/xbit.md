# xbit bit 计算

xbit 是确定性 bit/value/expression calculator。遇到 SV literal、slice、signed、mask、表达式或 expected value 比较时必须使用，不要心算。

## 何时使用

- 进制转换：hex/bin/decimal/SV literal。
- signed/unsigned：如 `8'shff`。
- bit slice/index、concat/repeat、trunc/zext/sext。
- popcount、onehot、mask、gray code。
- 常量表达式、valid-ready 条件、opcode/field 比较。
- xdebug 返回 `xbit_hints.commands[]` 或 `slice_hint`。

## 入口

已注册 MCP 时，直接在 Claude Code 中请求对应工具，不需要先运行 CLI。
`eval` 的表达式使用 decimal 或 SV literal；不要把单值转换支持的所有进制写法
类推到表达式语法。以下 MCP 调用示例分别返回 unsigned=17 和 matched=true：

```json
{"tool":"xverif_bit_eval","args":{"expr":"8'h10 + 8'h01","output_format":"json"}}
```

```json
{"tool":"xverif_bit_check","args":{"expr":"actual == expected","vars":{"actual":"8'h11","expected":"8'h11"},"output_format":"json"}}
```

`check.values` 是变量绑定 JSON 文件路径，不是预期数值；文件内容是直接的
变量名到 literal 的 map，且不能与 `vars` 同时传入。比较条件写在 `expr` 中；
`matched=false` 表示条件不成立，不代表工具执行失败。先检查 `ok`，再读 `matched`。

```text
Use xverif_bit_check to test whether actual equals expected.
Pass vars={"actual":"8'h11","expected":"8'h12"} and expr="actual == expected".
Report ok and matched from the response; do not treat a false condition as a tool error.
```

只有任务已选择 CLI surface 时使用以下命令；不要因 MCP 错误自动改用 CLI：

```bash
xbit conv "8'shff" --json
tools/xbit slice "32'hdead_beef" 15 8 --json
xbit eval "valid && ready" --var "valid=1'b1" --var "ready=1'b0" --json
```

## 读取规则

- 先看 `ok`。
- 结果读 `result.width/result.unsigned/result.signed_value/result.hex/result.bin/result.sv`。
- 条件读 `result.bool` 或 `matched`。
- `known:false` 不能当确定值。
- 错误时读 `error.code`，修输入宽度、literal 或表达式。

## 边界

xbit 不读 RTL、不做 elaboration、不查波形。需要事实先用 xdebug，拿到值后再用 xbit 算。
