# 自定义下载镜像教程

这个目录里的 `*.json` 纯规则文件和 `*.js` 动态脚本都会被启动器作为镜像规则加载。首次打开时，
启动器会把本教程、`mirror-example.json` 和 `mirror-example.js.txt` 模板复制到这里。模板使用
`.txt` 后缀，因此不会被立即执行；确认内容正确后，再把它复制或重命名为 `my-mirror.js`。

## 推荐：纯 JSON 规则

不需要计算时优先使用 JSON。它不会执行脚本，适合声明多个下载源和测速地址：

```json
{
  "metadata": { "id": "my-mirror", "kind": "mirror", "displayName": "我的镜像", "version": "1.0.0", "priority": 200 },
  "rules": [
    {
      "match": { "hosts": ["resources.download.minecraft.net"] },
      "rewrite": {
        "candidates": [
          { "base": "https://mirror-a.example/assets", "probeUrl": "https://mirror-a.example/", "priority": 10 },
          { "base": "https://mirror-b.example/assets", "probeUrl": "https://mirror-b.example/", "priority": 5 }
        ]
      }
    }
  ]
}
```

候选首次按 `priority` 使用，后台 HEAD 测速完成后切换到最近十分钟内延迟最低的候选。
`base`、`stripPrefix` 和 `template` 仍可单独使用；`template` 适合 `pathRegex` 捕获组替换。

## 最小规则

镜像脚本需要导出名为 `api` 的对象，并在 `metadata.kind` 中填写 `mirror`：

```js
var api = {
    metadata: {
        id: "my-mirror",
        kind: "mirror",
        displayName: "我的镜像",
        version: "1.0.0",
        enabled: true,
        priority: 200
    },

    rewriteUrl(url, context) {
        // 不认识的 URL 必须返回 null，让其他镜像规则继续尝试。
        if (!url.startsWith("https://resources.download.minecraft.net/"))
            return null;

        const path = url.slice("https://resources.download.minecraft.net/".length);
        return "https://mirror.example.com/assets/" + path;
    }
};
```

## `context` 中可用的配置

启动器会把当前镜像设置传给 `rewriteUrl(url, context)`，包括：

- `mirrorType`：`official`、`bmclapi`、`fastmcmirror`、`hkmc` 或 `custom`
- `mirrorTypeId`：内置类型的数字 ID；Custom 是 `2`
- `mojangBase`、`librariesBase`、`assetsBase`
- `fabricMetaBase`、`quiltMetaBase`、`fmlLibsBase`
- `authlibInjectorBase`、`modrinthCdnBase`

如果镜像使用 BMCLAPI 兼容目录，可以直接拼接 `context.assetsBase` 或
`context.librariesBase`，不要把 BMCLAPI 域名写死在通用脚本中。

## 安装和刷新

1. 在本目录创建或复制一个以 `.js` 结尾的文件。
2. 修改 `metadata.id`，每个脚本必须使用唯一 ID。
3. `rewriteUrl` 返回改写后的字符串表示接管下载；返回 `null` 表示继续交给下一个规则。
4. 保存文件后点击 JS API 页面中的“刷新列表”，即可重新扫描规则。新任务会使用新的排序和测速结果。

脚本加载失败时，请查看日志中的 `[JSAPIManager]` 行。建议先从
`mirror-example.js.txt` 复制一份，再逐步修改域名和路径。
