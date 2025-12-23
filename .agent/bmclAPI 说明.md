# BMCLAPI

## 关于

BMCLAPI 是 [@bangbang93](https://github.com/bangbang93) 开发的 BMCL 的一部分，用于解决国内线路对 Forge 和 Minecraft 官方使用的 Amazon S3 速度缓慢的问题。BMCLAPI 是对外开放的，所有需要 Minecraft 资源的启动器均可调用。

若有任何意见或者建议，可以去 [BMCL 板块发帖](https://www.bangbang93.com/category/6/bmclapi)。

推荐使用 BMCLAPI 的开发者关注 [BMCLAPI 的 TG 频道](https://t.me/bmclapi)以获得通知。

## 协议

- BMCLAPI 下的所有文件，除 BMCLAPI 本身的源码之外，归源站点所有
- BMCLAPI 会尽量保证文件的完整性、有效性和实时性，对于使用 BMCLAPI 带来的一切纠纷，与 BMCLAPI 无关
- BMCLAPI 和 BMCL 不同，属于非开源项目
- 所有使用 BMCLAPI 的程序必需在下载界面或其他可视部分标明来源
- 禁止在 BMCLAPI 上二次封装其他协议

---

## 捐助

现阶段的 BMCLAPI 服务器部分由以下赞助：

### 企业赞助
- AD-蓝科数据
- AD-AkiraCloud
- 毛玉线圈物语
- AD-RhyCloud 旋律工艺云计算
- AD-林枫云
- AD-云望IT
- AD-云望IT-乐青云
- AD-听风数据
- 星极世纪

### 高校源
以下高校目前正在为 BMCLAPI 提供部分开源项目的镜像服务，感谢他们的支持：

- 中国科学技术大学 Linux 用户协会
- 南京大学开源镜像站
- 兰州大学开源软件镜像站
- 齐鲁工业大学开源软件镜像站
- 更多高校详见校园网联合镜像站

### 个人赞助
[爱发电 - 完整赞助名单](https://afdian.net/a/bangbang93)

服务器的开销是有费用的，若你觉得 BMCLAPI 对你有帮助，欢迎捐助，支付宝：bangbang93@bangbang93.com

捐赠所得将用于后续的开发和维护。

---

## 使用

**如何使用 Mojang 和 Forge 官方源进行下载就不再赘述了，不知道的开发者请自行 Google。以下所有内容均建立在已经能够成功从官方源下载数据的基础上。**

BMCLAPI 的目标是 100% 兼容官方的文件目录结构，不过由于是多源合一，所以部分资源的根路径会有所区别。

熟悉 C# 的朋友可以参考 [BMCL 的 Mirror 实现](https://github.com/bangbang93/BMCL/tree/master/BMCLV2/Mirrors)。

### 基础资源映射

| 资源类型 | 官方源 | BMCLAPI |
|---------|--------|---------|
| **版本信息** | `http://launchermeta.mojang.com/mc/game/version_manifest.json` | `https://bmclapi2.bangbang93.com/mc/game/version_manifest.json` |
| **版本信息 v2** | `http://launchermeta.mojang.com/mc/game/version_manifest_v2.json` | `https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json` |
| **版本和版本 JSON 以及 AssetsIndex** | 将版本信息内的 URL 中的 `https://launchermeta.mojang.com/` 和 `https://launcher.mojang.com/` 替换为 `https://bmclapi2.bangbang93.com` | |
| **Assets** | `http://resources.download.minecraft.net` | `https://bmclapi2.bangbang93.com/assets` |
| **Libraries** | `https://libraries.minecraft.net/` | `https://bmclapi2.bangbang93.com/maven` |
| **Mojang Java** | `https://launchermeta.mojang.com/v1/products/java-runtime/.../all.json` | `https://bmclapi2.bangbang93.com/v1/products/java-runtime/.../all.json` |
| **Forge** | `https://files.minecraftforge.net/maven` | `https://bmclapi2.bangbang93.com/maven` |
| **Liteloader** | `http://dl.liteloader.com/versions/versions.json` | `https://bmclapi.bangbang93.com/maven/com/mumfrey/liteloader/versions.json` |
| **Optifine** | 请遵照 API，无官方结构 | |
| **authlib-injector** | `https://authlib-injector.yushi.moe` | `https://bmclapi2.bangbang93.com/mirrors/authlib-injector` |
| **Fabric** | `https://meta.fabricmc.net` | `https://bmclapi2.bangbang93.com/fabric-meta` |
| **Fabric Maven** | `https://maven.fabricmc.net` | `https://bmclapi2.bangbang93.com/maven` |
| **NeoForge** | `https://maven.neoforged.net/releases/net/neoforged/forge` | `https://bmclapi2.bangbang93.com/maven/net/neoforged/forge` |
| **NeoForge (neoforge)** | `https://maven.neoforged.net/releases/net/neoforged/neoforge` | `https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge` |
| **Quilt** (上游API存在bug，暂时不可用) | `https://maven.quiltmc.org/repository/release` | `https://bmclapi2.bangbang93.com/maven` |
| **Quilt Meta** | `https://meta.quiltmc.org` | `https://bmclapi2.bangbang93.com/quilt-meta` |

---

## API 接口

### Forge API

#### 1. 下载 Forge

```
GET https://bmclapi2.bangbang93.com/forge/download
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| mcversion | String | mc 版本 |
| version | String | forge 版本 |
| branch | String | 分支 |
| category | String | 下载的文件类型 |
| format | String | 下载的文件格式 |

该接口不会直接返回下载文件，而是进行一次 302 重定向到真正的下载地址。

---

#### 2. 根据 build 下载 Forge

```
GET https://bmclapi2.bangbang93.com/forge/download/:build
```

返回 302 重定向到真正的下载地址。

---

#### 3. 根据版本获取 Forge 列表

```
GET https://bmclapi2.bangbang93.com/forge/minecraft/:id
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| id | String | minecraft 版本 |

**响应示例：**

```json
[
    {
        "branch": "1.9",
        "build": 1766,
        "mcversion": "1.9",
        "modified": "2016-03-18T07:44:28.000Z",
        "version": "12.16.0.1766",
        "_id": "57047535e914dfb05c6a346f",
        "files": [
            {
                "format": "zip",
                "category": "mdk",
                "hash": "a6612cab2c4ae3c3bba0acc089bbffc1",
                "_id": "57047535e914dfb05c6a3475"
            }
        ]
    }
]
```

---

#### 4. 获取 Forge 列表

```
GET https://bmclapi2.bangbang93.com/forge/list/:offset/:limit
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| offset | Number | 跳过多少个 build |
| limit | Number | 最多返回多少个 build (<=500) |

---

#### 5. 获取 Forge 支持的 Minecraft 版本列表

```
GET https://bmclapi2.bangbang93.com/forge/minecraft
```

**响应示例：**

```json
[
    "1.4.7",
    "1.7.2",
    "1.6.4",
    "1.1",
    "1.2.3",
    ...
]
```

---

#### 6. 获取最新版的 Forge

```
GET https://bmclapi2.bangbang93.com/forge/last
```

---

#### 7. 获取标记的 Forge 版本

```
GET https://bmclapi2.bangbang93.com/forge/promos
```

---

### Java API

#### 获取 Java 列表

```
GET https://bmclapi2.bangbang93.com/java/list
```

本接口可以视作是 <https://java.com/zh_CN/download/manual.jsp> 的序列化结果，缓存了 Windows、Mac OSX 和 Linux 下的 jre 安装包。Solaris 由于不是 bmclapi 的目标用户，所以不进行缓存。

本接口不保存历史结果，永远只保留最新的 jre，由于同步延迟，最长可能延迟 24 小时更新。

本接口返回的文件名可以直接用于下载，例如：`https://bmclapi.bangbang93.com/java/jre_x64.exe`

---

### Liteloader API

#### 1. 下载 Liteloader

```
GET https://bmclapi2.bangbang93.com/liteloader/download
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| version | String | 下载的版本，对应上面接口的 version 字段 |

返回 302 重定向到真正的下载地址。

也可以不调用该接口，直接手工拼接 `/maven/com/mumfrey/liteloader/${mcversion}/liteloader-${version}.jar` 进行下载。

---

#### 2. 原 Liteloader versions.json 镜像

```
GET https://bmclapi2.bangbang93.com/maven/com/mumfrey/liteloader/versions.json
```

---

#### 3. 获取 Liteloader 列表

```
GET https://bmclapi2.bangbang93.com/liteloader/list
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| mcversion | String | 特定的 Minecraft 版本，若不传将会返回所有 |

---

### Mirrors API

#### authlib-injector

同步源 <https://github.com/yushijinhun/authlib-injector.yushi.moe>，请阅读相关项目文档。

```
GET https://bmclapi2.bangbang93.com/mirrors/authlib-injector
```

---

### NeoForge API

#### 1. 下载 NeoForge 文件

根据 NeoForge 版本和文件名下载 NeoForge 文件。

```
GET https://bmclapi2.bangbang93.com/neoforge/version/:version/download/:file
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| version | String | neoforge 版本 |
| file | String | 文件类型：install \| installer.jar \| universal \| universal.jar \| mdk.zip \| userdev.jar |

返回 302 重定向到下载地址。

---

#### 2. 获取 NeoForge Maven API

```
GET https://bmclapi2.bangbang93.com/neoforge/meta/*
```

对应上游：`https://maven.neoforged.net/api/maven/details/releases/net/neoforged/{neoforge,forge}`

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| path | String | `/api/maven/details/releases/net/neoforged/{neoforge,forge}` |

---

#### 3. 获取 NeoForge 列表

根据 Minecraft 版本获取 NeoForge 列表。

```
GET https://bmclapi2.bangbang93.com/neoforge/list/:mcversion
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| mcversion | String | minecraft 版本 |

**响应示例：**

```json
[
    {
        "rawVersion": "1.20.1-47.1.12",
        "version": "47.1.12",
        "mcversion": "1.20.1"
    }
]
```

---

#### 4. 获取 NeoForge 版本信息

根据 NeoForge 版本获取 NeoForge 信息。

```
GET https://bmclapi2.bangbang93.com/neoforge/version/:version
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| version | String | neoforge 版本 |

---

### Optifine API

#### 1. 下载 Optifine

```
GET https://bmclapi2.bangbang93.com/optifine/:mcversion/:type/:patch
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| mcversion | String | mc 版本 |
| type | String | optifine 的种类，通常为 HD_U |
| patch | String | optifine 的补丁版本号，如 A1、A2、B1、B2 等 |

返回 302 重定向到实际下载地址。

**响应：**

- `302` - 跳转下载地址
- `404` - 没有找到匹配的 optifine

```json
{
    "msg": "no such optifine"
}
```

---

#### 2. 获取 Optifine 列表

```
GET https://bmclapi2.bangbang93.com/optifine/:mcversion
```

**参数：**

| 字段 | 类型 | 描述 |
|------|------|------|
| mcversion | String | mc 版本 |

**响应示例：**

```json
[
    {
        "mcversion": "1.10.2",
        "type": "HD_U_D2",
        "patch": "pre",
        "_id": "580de1ffb2d0720e29296bb9",
        "__v": 0
    }
]
```

---

#### 3. 获取全部 Optifine 列表

获取所有版本的 optifine。

```
GET https://bmclapi2.bangbang93.com/optifine/versionList
```

---

### Sponsor API

#### 随机获取一个赞助商

```
GET https://bmclapi2.bangbang93.com/openbmclapi/sponsor
```

**响应示例：**

```json
{
    "_id": "65e1b657319eecff4a60e60f",
    "link": "https://bd.bangbang93.com/pages/rank/sponsor/65e1b657319eecff4a60e60f",
    "name": "OpenBMCLAPI"
}
```

---

### Version API

#### 下载 MC 本体文件

```
GET https://bmclapi2.bangbang93.com/version/:version/:category
```

**示例：**

- 下载客户端：`/version/1.7.10/client`
- 下载服务端：`/version/1.7.10/server`
- 下载 json：`/version/1.7.10/json`

返回 302 重定向到下载地址。

---

## OpenBMCLAPI

这个项目的主要目的是辅助 BMCLAPI 分发文件。

详见：[https://github.com/bangbang93/openbmclapi](https://github.com/bangbang93/openbmclapi)

### 对节点的要求

对节点的要求降低了不少：
- 公网可访问（端口映射也可），可以非 80
- 10Mbps 以上的上行速度
- 如果在国外，则要对国内速度友好
- 可以长时间稳定在线
- 暂不支持 IPv6

如果你用过 ehentai 的 H@H 项目，可能会觉得比较熟悉。

若有意，请去论坛回复。
