# aresin BPF Backport 系列（53 patches）

来源：[jsdizkcksv/android_kernel_aresin](https://github.com/jsdizkcksv/android_kernel_aresin)
提交区间 `a2b662d..b9af68f`，即 aresin 作者把 4.14 内核 BPF 补到 4.19+ 水平的完整系列，
已在其设备上验证可运行 Android 15。编号按该树上的时间正序排列，`apply-all.sh` 依次应用。

## 与你现有补丁（/workspace/a15-bpf/v3）的取舍

| 现有补丁 | 处理 | 原因 |
|---|---|---|
| 0001 fake-uname | **撤掉**，用本系列 0053 | 同一文件 `kernel/sys.c`，aresin 版伪装 4.19.280，二选一 |
| 0002 sock-addr 类型 | **保留** | 本系列不含 CGROUP_SOCK_ADDR 注册（aresin 树里在 a2b662d 之前） |
| 0003 net hooks | **撤掉** | 被 0030-0033（bind/connect/post-bind/sendmsg hooks + `__inet_bind`）完整覆盖 |
| 0004 位域修复 | **保留** | aresin 的 sock_addr 修复不在这个区间，chopin 仍需要 |
| 0005 get_func_proto 签名 | **保留** | 同上 |
| 0006 devkmsg 诊断 | 调试完撤掉 | 与本系列无关 |
| 0007 忽略 bpf_attr 尾部 | **撤掉** | 被 0001-0003（map_name/prog_name 真实现）取代 |

推荐流程：**从干净 chopin 基线开始**（不要叠在已打过 0002~0007 的树上）：

```bash
# 1) 干净基线
git am aresin-series/0*.patch        # 或 bash aresin-series/apply-all.sh aresin-series
# 2) 再补上
patch -p1 < 0002-bpf-cgroup-sock-addr-type.patch
patch -p1 < 0004-bpf-sock-addr-type-protocol-bitfield-fix.patch
patch -p1 < 0005-bpf-get-func-proto-signature-fix.patch
```

若用 `git am`，遇到冲突可加 `--3way`。

## 系列内容（53 个，按序）

| 编号 | 主题 | 说明 |
|---|---|---|
| 0001-0003 | map/prog **name** | 真实现 `bpf_attr` 的 map_name/prog_name（A15 loader 传的就是这个）；取代 0007 |
| 0004 | skb_load_bytes_relative | 4.20 helper，A15 程序使用 |
| 0005-0007 | verifier log 重构 | `bpf_verifier_log` 结构体，BTF 的前置 |
| 0007-0027 | **BTF 全家桶** | 引入 BTF（`c330115`）、`BPF_BTF_LOAD`、OBJ_GET_INFO、uapi 清理与十余个修复，共约 20 个 |
| 0028-0029 | devmap | `BPF_MAP_TYPE_DEVMAP_HASH`（4.18），tethering offload 需要 |
| 0030-0033 | **网络 hooks** | sys_bind / sys_connect / post-bind / sys_sendmsg，含 `__inet_bind/__inet6_bind` 拆分 |
| 0034-0036 | XDP/offload | ndo_xdp→ndo_bpf 改名 + offload 基础设施 |
| 0037-0038 | PROG_QUERY / attach_type | A15 loader 会用到 |
| 0039-0050 | 杂项修复 | cgroup-bpf return code、udp hooks、lpm、map_fd_put_ptr 等 |
| 0051-0052 | JIT 杂项 | `__weak` exec memory 分配 hook、无模块支持下启用 JIT |
| 0053 | uname 伪装 4.19.280 | 与现有 0001 二选一 |

## 预期会失败的 hunk（可忽略）

0034-0036 改了大量**服务器网卡驱动**（bnxt、i40e、ixgbe、mlx4/5、nfp、qede、thunder、
virtio_net）。手机内核没有这些文件，`patch` 会对它们报错并留下 `.rej`——
**这些 .rej 直接删掉即可**。关键部分（`include/linux/netdevice.h`、tun、veth 等）
会正常应用，offload 基础设施照常生效。

## 编译配置

本系列不改 Kconfig，沿用之前确认过的配置即可（`CONFIG_BPF_SYSCALL`、
`CONFIG_CGROUP_BPF`、`CONFIG_BPF_JIT` 等）。若 `CONFIG_MODULES` 未开启，
0052 会允许 JIT 继续工作。

## 提醒

- aresin 基线是 4.14.328，chopin 是 4.14.186，`kernel/bpf/verifier.c` 等文件
  可能有个别 hunk 冲突，`--3way` 或手工合并即可，冲突点集中在这几个文件。
- 打完后先编 0004/0005 依赖的 `net/core/filter.c`，确认 sock_addr 部分没有
  与 aresin 版本重复定义。
- 0006（devkmsg 诊断）建议保留到 A15 首次启动成功后再撤。
