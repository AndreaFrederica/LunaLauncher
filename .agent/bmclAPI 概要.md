BMCLAPI
关于
BMCLAPI是@bangbang93开发的BMCL的一部分，用于解决国内线路对Forge和Minecraft官方使用的Amazon S3 速度缓慢的问题。BMCLAPI是对外开放的，所有需要Minecraft资源的启动器均可调用。

若有任何意见或者建议，可以去BMCL板块发帖https://www.bangbang93.com/category/6/bmclapi

推荐使用BMCLAPI的开发者关注BMCLAPI的TG频道以获得通知https://t.me/bmclapi

协议
BMCLAPI下的所有文件，除BMCLAPI本身的源码之外，归源站点所有
BMCLAPI会尽量保证文件的完整性、有效性和实时性，对于使用BMCLAPI带来的一切纠纷，与BMCLAPI无关。
BMCLAPI和BMCL不同，属于非开源项目
所有使用BMCLAPI的程序必需在下载界面或其他可视部分标明来源
禁止在BMCLAPI上二次封装其他协议
捐助
现阶段的BMCLAPI服务器部分由以下赞助
AD-蓝科数据
AD-AkiraCloud
毛玉线圈物语
AD-RhyCloud 旋律工艺云计算
AD-林枫云
AD-云望IT
AD-云望IT-乐青云
AD-听风数据
星极世纪
高校源
以下高校目前正在为BMCLAPI提供部分开源项目的镜像服务，感谢他们的支持

中国科学技术大学 Linux 用户协会
南京大学开源镜像站
兰州大学开源软件镜像站
齐鲁工业大学开源软件镜像站
更多高校详见校园网联合镜像站

爱发电

完整赞助名单

服务器的开销是有费用的，若你觉得BMCLAPI对你有帮助，欢迎捐助，支付宝：bangbang93@bangbang93.com

↓或者扫码↓

先领个红包





捐赠所得将用于后续的开发和维护，如有剩余，可能会用来买可爱的女装

使用
如何使用Mojang和Forge官方源进行下载就不再赘述了，不知道的开发者请自行Google。 以下所有内容均建立在已经能够成功从官方源下载数据的基础上

BMCLAPI的目标是100%兼容官方的文件目录结构，不过由于是多源合一，所以部分资源的根路径会有所区别。 熟悉C#的朋友可以参考BMCL的Mirror实现https://github.com/bangbang93/BMCL/tree/master/BMCLV2/Mirrors

版本信息
http://launchermeta.mojang.com/mc/game/version_manifest.json -> https://bmclapi2.bangbang93.com/mc/game/version_manifest.json
http://launchermeta.mojang.com/mc/game/version_manifest_v2.json -> https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json
版本和版本JSON以及AssetsIndex
将版本信息内的URL中的https://launchermeta.mojang.com/ 和 https://launcher.mojang.com/ 替换为 https://bmclapi2.bangbang93.com
Assets
http://resources.download.minecraft.net -> https://bmclapi2.bangbang93.com/assets
Libraries
https://libraries.minecraft.net/ -> https://bmclapi2.bangbang93.com/maven
Mojang java
https://launchermeta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json -> https://bmclapi2.bangbang93.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json
Forge
https://files.minecraftforge.net/maven -> https://bmclapi2.bangbang93.com/maven
Liteloader
http://dl.liteloader.com/versions/versions.json -> https://bmclapi.bangbang93.com/maven/com/mumfrey/liteloader/versions.json
Optifine
请遵照API，无官方结构
authlib-injector
https://authlib-injector.yushi.moe -> https://bmclapi2.bangbang93.com/mirrors/authlib-injector
fabric
https://meta.fabricmc.net -> https://bmclapi2.bangbang93.com/fabric-meta
https://maven.fabricmc.net -> https://bmclapi2.bangbang93.com/maven
neoforge
https://maven.neoforged.net/releases/net/neoforged/forge -> https://bmclapi2.bangbang93.com/maven/net/neoforged/forge
https://maven.neoforged.net/releases/net/neoforged/neoforge -> https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge
quilt (由于上游API存在bug，暂时不可用)
https://maven.quiltmc.org/repository/release -> https://bmclapi2.bangbang93.com/maven
https://meta.quiltmc.org -> https://bmclapi2.bangbang93.com/quilt-meta
OpenBMCLAPI
这个项目的主要目的是辅助bmclapi分发文件

详见 https://github.com/bangbang93/openbmclapi

对节点的要求降低了不少

公网可访问（端口映射也可），可以非80
10Mbps以上的上行速度
如果在国外，则要对国内速度友好
可以长时间稳定在线
暂不支持IPv6
如果你用过ehentai的H@H项目，可能会觉得比较熟悉

若有意，请去论坛回复

Forge
Forge | 下载forge
这个接口不会直接返回下载文件,而是进行一次302重定向到真正的下载地址 下载地址拼接方法 return '/maven/net/minecraftforge/forge/' + mcversion + '-' + version + (branch?-${branch}:'') + '/forge-' + mcversion + '-' + version + (branch?-${branch}:'') + '-' + category + '.' + format;

get
https://bmclapi2.bangbang93.com/forge/download
参数
字段	类型	描述
mcversion	String
mc版本

version	String
forge版本

branch	String
分支

category	String
下载的文件类型

format	String
下载的文件格式

发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/download
参数

ajax-auto
mcversion
String
version
String
branch
String
category
String
format
String

Forge | 根据build下载forge
这个接口不会直接返回下载文件,而是进行一次302重定向到真正的下载地址 下载地址拼接方法 return '/maven/net/minecraftforge/forge/' + mcversion + '-' + version + (branch?-${branch}:'') + '/forge-' + mcversion + '-' + version + (branch?-${branch}:'') + '-' + category + '.' + format;

get
https://bmclapi2.bangbang93.com/forge/download/:build
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/download/:build

Forge | 根据版本获取forge列表
get
https://bmclapi2.bangbang93.com/forge/minecraft/:id
参数
字段	类型	描述
id	String
minecraft版本

(200) {json} forgeBuilds
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
            },
            {
                "format": "txt",
                "category": "changelog",
                "hash": "e67f1af901089faf424b5e98b02d44a9",
                "_id": "57047535e914dfb05c6a3474"
            },
            {
                "format": "jar",
                "category": "universal",
                "hash": "da088a119849ea5cee274d116e2614b1",
                "_id": "57047535e914dfb05c6a3473"
            },
            {
                "format": "jar",
                "category": "userdev",
                "hash": "f681db67f356e912a7baff1fa440c2a4",
                "_id": "57047535e914dfb05c6a3472"
            },
            {
                "format": "exe",
                "category": "installer-win",
                "hash": "ee4cce09bcfd70aa87884c5c96f46871",
                "_id": "57047535e914dfb05c6a3471"
            },
            {
                "format": "jar",
                "category": "installer",
                "hash": "e4e0b0e1af095519e21de667f6f30b33",
                "_id": "57047535e914dfb05c6a3470"
            }
        ]
    }
]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/minecraft/:id
参数

ajax-auto
id
String

Forge | 获取forge列表
/forge/list/:limit

get
https://bmclapi2.bangbang93.com/forge/list/:offset/:limit
参数
字段	类型	描述
offset	Number
跳过多少个build

limit	Number
最多返回多少个build,<=500

(200) {json} forgeBuild
[
    {
        "branch": null,
        "build": 530,
        "mcversion": "1.4.7",
        "modified": "2014-09-29T12:22:37.000Z",
        "version": "6.6.1.530",
        "_id": "57047533e914dfb05c6a182b",
        "files": [
            {
                "format": "zip",
                "category": "src",
                "hash": "70fc679fdb0764699d25629753829a08",
                "_id": "57047533e914dfb05c6a182e"
            },
            {
                "format": "zip",
                "category": "universal",
                "hash": "7a15548642747015214f3878ac0f407e",
                "_id": "57047533e914dfb05c6a182d"
            },
            {
                "format": "zip",
                "category": "javadoc",
                "hash": "ab7cbd9b717b288312e6988ace292b10",
                "_id": "57047533e914dfb05c6a182c"
            }
        ]
    }
]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/list/:offset/:limit
参数

ajax-auto
offset
Number
limit
Number

Forge | 获取forge支持的minecraft版本列表
get
https://bmclapi2.bangbang93.com/forge/minecraft
(200) {json} minecraft版本
[
    "1.4.7",
    "1.7.2",
    "1.6.4",
    null,
    "1.1",
    "1.2.3",
    "1.2.4",
    "1.2.5",
    "1.3.2",
    "1.4.0",
    "1.4.1",
    "1.4.2",
    "1.4.3",
    "1.4.4",
    "1.4.5",
    "1.4.6",
    "1.5",
    "1.5.1",
    "1.5.2",
    "1.6.1",
    "1.6.2",
    "1.6.3",
    "1.7.10_pre4",
    "1.7.10",
    "1.8",
    "1.8.8",
    "1.8.9",
    "1.9"
]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/minecraft

Forge | 获取最新版的forge
/forge/latest

get
https://bmclapi2.bangbang93.com/forge/last
(200) {json} forgebuild
{
    "_id": "56d7ddb4128605f4a734698c",
    "build": {
        "branch": null,
        "build": 1847,
        "mcversion": "1.8.9",
        "modified": "2016-04-05T20:39:12.000Z",
        "version": "11.15.1.1847",
        "_id": "57047535e914dfb05c6a369f",
        "files": [
            {
                "format": "zip",
                "category": "mdk",
                "hash": "27afba09d41f382deabf812084425ca4",
                "_id": "57047535e914dfb05c6a36a5"
            },
            {
                "format": "txt",
                "category": "changelog",
                "hash": "e008ac32e0744ab456a1b253261049e1",
                "_id": "57047535e914dfb05c6a36a4"
            },
            {
                "format": "jar",
                "category": "universal",
                "hash": "ac579f7dcf6cd5470cf6d6146be8d2a4",
                "_id": "57047535e914dfb05c6a36a3"
            },
            {
                "format": "jar",
                "category": "userdev",
                "hash": "ebd6523e76ff49e9bdcdaf116b39df91",
                "_id": "57047535e914dfb05c6a36a2"
            },
            {
                "format": "exe",
                "category": "installer-win",
                "hash": "7bab432a47e211a426750b7876c04c7a",
                "_id": "57047535e914dfb05c6a36a1"
            },
            {
                "format": "jar",
                "category": "installer",
                "hash": "f3b79bd8b0762b8d51988d2591dcf82e",
                "_id": "57047535e914dfb05c6a36a0"
            }
        ]
    },
    "name": "latest"
}
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/last

Forge | 获取标记的forge版本
get
https://bmclapi2.bangbang93.com/forge/promos
(200) {json} forgebuilds
{
  "_id": "56d7ddb4128605f4a734697a",
  "build": {
    "branch": null,
    "build": 738,
    "mcversion": "1.5.2",
    "modified": "2014-09-29T10:47:43.000Z",
    "version": "7.8.1.738",
    "_id": "57047534e914dfb05c6a1b39",
    "files": [
      {
        "format": "jar",
        "category": "installer",
        "hash": "f0929a34f4ddc1ad7d83efa30e785980",
        "_id": "57047534e914dfb05c6a1b3e"
      },
      {
        "format": "zip",
        "category": "universal",
        "hash": "8889a0e9fa22a71f4dfa63aab16c495b",
        "_id": "57047534e914dfb05c6a1b3d"
      },
      {
        "format": "zip",
        "category": "src",
        "hash": "82d0c238e13dda7b62d7c9ebb8714866",
        "_id": "57047534e914dfb05c6a1b3c"
      },
      {
        "format": "zip",
        "category": "javadoc",
        "hash": "2dee28f94997a6ba337084dcff4e1927",
        "_id": "57047534e914dfb05c6a1b3b"
      },
      {
        "format": "txt",
        "category": "changelog",
        "hash": "b4584fe2d790e0748a22247b5be81dc2",
        "_id": "57047534e914dfb05c6a1b3a"
      }
    ]
  },
  "name": "1.5.2-latest"
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/forge/promos

Java
Java | 获取java列表
本接口可以视作是https://java.com/zh_CN/download/manual.jsp的序列化结果，缓存了Windows,Mac OSX和Linux下的jre安装包， Solaris由于不是bmclapi的目标用户，所以不进行缓存。

本接口不保存历史结果，永远只保留最新的jre，由于同步延迟，最长可能延迟24小时更新

本接口返回的文件名可以直接用户下载，例如https://bmclapi.bangbang93.com/java/jre_x64.exe

get
https://bmclapi2.bangbang93.com/java/list
请求成功（200）
字段	类型	描述
body	Array
已缓存的java列表

title	String
名称

file	String
文件名

发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/java/list

Liteloader
Liteloader | 下载liteloader
get
https://bmclapi2.bangbang93.com/liteloader/download
参数
字段	类型	描述
version	String
下载的版本，对应上面接口的version字段

(302) 重定向下载
重定向到真正的下载地址，
也可以不调用该接口，直接手工拼接`/maven/com/mumfrey/liteloader/${mcversion}/liteloader-${version}.jar`进行下载
但是SNAPSHOT版需要按照`/maven/com/mumfrey/liteloader/${version}/liteloader-${version}.jar`进行拼接
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/liteloader/download
参数

ajax-auto
version
String

Liteloader | 原liteloader versions.json镜像
get
https://bmclapi2.bangbang93.com/maven/com/mumfrey/liteloader/versions.json
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/maven/com/mumfrey/liteloader/versions.json

Liteloader | 获取liteloader列表
get
https://bmclapi2.bangbang93.com/liteloader/list
参数
字段	类型	描述
mcversion	String
特定的Minecraft版本，若不传将会返回所有

(200) {JSON} 列表
[
    {
        "__v": 0,
        "_id": "57ec9db960ca1244ac844b92",
        "build": {
            "file": "liteloader-1.7.10.jar",
            "libraries": [
                {
                    "name": "net.minecraft:launchwrapper:1.11"
                },
                {
                    "name": "org.ow2.asm:asm-all:5.0.3"
                }
            ],
            "mcpJar": "liteloader-1.7.10_04-mcpnames.jar",
            "md5": "63ada46e033d0cb6782bada09ad5ca4e",
            "srcJar": "liteloader-1.7.10_04-mcpnames-sources.jar",
            "stream": "RELEASE",
            "timestamp": "1414368553",
            "tweakClass": "com.mumfrey.liteloader.launch.LiteLoaderTweaker",
            "version": "1.7.10_04"
        },
        "hash": "63ada46e033d0cb6782bada09ad5ca4e",
        "mcversion": "1.7.10",
        "type": "RELEASE",
        "version": "1.7.10_04"
    }
]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/liteloader/list
参数

ajax-auto
mcversion
String

Mirrors
Mirrors | authlib-injector
同步源https://github.com/yushijinhun/authlib-injector.yushi.moe，请阅读相关项目文档

get
https://bmclapi2.bangbang93.com/mirrors/authlib-injector
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/mirrors/authlib-injector

Neoforge
Neoforge | 下载neoforge文件
根据neoforge版本和文件名下载neoforge文件

get
https://bmclapi2.bangbang93.com/neoforge/version/:version/download/:file
参数
字段	类型	描述
version	String
neoforge版本

file	String
文件类型 install | installer.jar | universal | universal.jar | mdk.zip | userdev.jar

(302) neoforge
Redirect to download url
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/neoforge/version/:version/download/:file
参数

ajax-auto
version
String
file
String

Neoforge | 获取neoforge maven api
https://maven.neoforged.net/api/maven/details/releases/net/neoforged/{neoforge,forge}

get
https://bmclapi2.bangbang93.com/neoforge/meta/*
参数
字段	类型	描述
path	String
/api/maven/details/releases/net/neoforged/{neoforge,forge}

(200) json
{}
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/neoforge/meta/*
参数

ajax-auto
path
String

Neoforge | 获取neoforge列表
根据minecraft版本获取neoforge列表

get
https://bmclapi2.bangbang93.com/neoforge/list/:mcversion
参数
字段	类型	描述
mcversion	String
minecraft版本

(200) {json} neoforge
[
{
 "rawVersion": "1.20.1-47.1.12",
 "version": "47.1.12",
 "mcversion": "1.20.1",
 }
 ]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/neoforge/list/:mcversion
参数

ajax-auto
mcversion
String

Neoforge | 获取neoforge版本信息
根据neoforge版本获取neoforge信息

get
https://bmclapi2.bangbang93.com/neoforge/version/:version
参数
字段	类型	描述
version	String
neoforge版本

(200) {json} neoforge
{
"rawVersion": "1.20.1-47.1.12",
"version": "47.1.12",
"mcversion": "1.20.1",
}
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/neoforge/version/:version
参数

ajax-auto
version
String

Optifine
Optifine | 下载optifine
该接口也不会直接提供文件下载，而是302到实际下载地址，所以也可以手工拼写 /maven/com/optifine/${mcversion}/Optifine_${mcversion}_${type}_${patch}.jar 直接下载，其实这三个字段也就对应着文件名的三个部分 但是从1.12开始，optifine又有了新文件名规则……所以推荐使用api下载

get
https://bmclapi2.bangbang93.com/optifine/:mcversion/:type/:patch
参数
字段	类型	描述
mcversion	String
mc版本

type	String
optifine的种类，不过从bmclapi2开始提供optifine开始，似乎只能下载到OptiFine HD Ultra这一个类型了，所以 这里应该会一直保持为HD_U

patch	String
optifine的补丁版本号，就是后面常见的A1A2,B1B2和C1C2之类的

(302) 跳转下载地址
HTTP/1.1 302 Redirect
(404) 没有找到匹配的optifine
{
    "msg": "no such optifine"
}
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/optifine/:mcversion/:type/:patch
参数

ajax-auto
mcversion
String
type
String
patch
String

Optifine | 获取optifine列表
get
https://bmclapi2.bangbang93.com/optifine/:mcversion
参数
字段	类型	描述
mcversion	String
mc版本

版本列表
[
    {
        "mcversion": "1.10.2",
        "type": "HD_U_D2",
        "patch": "pre",
        "_id": "580de1ffb2d0720e29296bb9",
        "__v": 0
    }
]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/optifine/:mcversion
参数

ajax-auto
mcversion
String

Optifine | 获取全部optifine列表
获取所有版本的optifine

get
https://bmclapi2.bangbang93.com/optifine/versionList
版本列表
[
    {
        "mcversion": "1.10.2",
        "type": "HD_U_D2",
        "patch": "pre",
        "_id": "580de1ffb2d0720e29296bb9",
        "__v": 0
    }
]
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/optifine/versionList

Sponsor
Sponsor | 随机获取一个赞助商
get
https://bmclapi2.bangbang93.com/openbmclapi/sponsor
(200) {json} SponsorRandomOneResDto
{
    "_id": "65e1b657319eecff4a60e60f",
    "link": "https://bd.bangbang93.com/pages/rank/sponsor/65e1b657319eecff4a60e60f",
    "name": "OpenBMCLAPI"
}
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/openbmclapi/sponsor

Version
Version | 下载mc本体文件
get
https://bmclapi2.bangbang93.com/version/:version/:category
下载客户端
下载服务端
下载json
/version/1.7.10/client
(302) 重定向到下载地址
HTTP/1.1 302 Found
Connection: keep-alive
Content-Length: 96
Content-Type: text/plain; charset=utf-8
Date: Thu, 07 Apr 2016 07:38:11 GMT
Location: /mc/game/1.7.10/server/952438ac4e01b4d115c5fc38f891710c4941df29/server.jar
Vary: Accept
X-Content-Type-Options: nosniff
X-Download-Options: noopen
X-Frame-Options: SAMEORIGIN
X-XSS-Protection: 1; mode=block

Found. Redirecting to /mc/game/1.7.10/server/952438ac4e01b4d115c5fc38f891710c4941df29/server.jar
发送示例请求

URL
地址
https://bmclapi2.bangbang93.com/version/:version/:category

构建于 apidoc 1.2.0 - Fri Jun 06 2025 07:03:45 GMT+0000 (Coordinated Universal Time)
