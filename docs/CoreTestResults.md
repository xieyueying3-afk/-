# 核心测试结果

## 实测环境

- 日期：2026-10-08（UTC）。
- 平台：Linux，GCC 14.2.0，CMake 4.4.4，Debug 构建。
- 仓库：`/workspace/-`；分支：`feature/core-backup`。
- 本环境原无 CMake，使用 pip 安装到 `/tmp/core-backup-tools`，未修改仓库原有文件。

## 执行命令

```bash
pip install --target /tmp/core-backup-tools cmake
/tmp/core-backup-tools/cmake/data/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
/tmp/core-backup-tools/cmake/data/bin/cmake --build build --parallel 2
/tmp/core-backup-tools/cmake/data/bin/ctest --test-dir build --output-on-failure -V
# 也可独立运行：
./build/core_backup_tests
```

已有 CMake 的机器可直接用 cmake 与 ctest，无需上述临时安装路径。

## 测试记录

| 用例 | 验证内容 | 结果 |
| --- | --- | --- |
| 1 | 多级备份、二进制内容、中文路径、空子目录、统计 | 通过 |
| 2 | 完整还原，比较全部相对路径与文件内容 | 通过 |
| 3 | 空根目录备份与还原 | 通过 |
| 4 | 重复备份拒绝已有文件，已有备份内容不变 | 通过 |
| 5 | 显式覆盖备份和还原，验证新内容 | 通过 |
| 6 | 筛选目录剪枝、跳过统计、逐文件进度 | 通过 |
| 7 | 源不存在、源为文件、目标为空，非法操作不创建目标 | 通过 |
| 8 | 同路径、目标为源子目录、目标为源祖先 | 通过 |
| 9 | 目标根为文件、目标文件位置为目录 | 通过 |
| 10 | FileUtils 遍历、创建、文件复制、自身覆盖冲突 | 通过 |
| 11 | 回调异常向上传递 | 通过 |
| 12 | 源符号链接、目标根及内部符号链接 | 通过 |
| 13 | 硬链接指向同一文件时拒绝覆盖 | 通过 |
| 14 | 无权限源目录和无权限目标目录 | 通过 |

```text
Results: 14 passed, 0 failed, 0 skipped assertions
100% tests passed, 0 tests failed out of 1
```

CTest 注册一个测试可执行程序，程序内部执行上述 14 组用例。
测试使用唯一临时目录，结束时自动清理，不修改真实用户目录。
权限测试在能绕过权限位的账号下会明确打印 SKIP，本次执行没有跳过权限断言。
符号链接、硬链接与权限位测试在 Windows 下不执行；Windows 编译及 Qt 集成尚未实测。
磁盘耗尽、并发修改与大规模目录性能不在本次验证范围内。
