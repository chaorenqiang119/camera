# GitHub Pages 项目展示页

访问地址：<https://chaorenqiang119.github.io/camera/>，已于 2026-10-04 成功发布。

部署记录：[Deploy project website](https://github.com/chaorenqiang119/camera/actions/runs/37201877929)。已检查首页、CSS、JavaScript、favicon 和原始日志下载均返回 HTTP 200，线上静态资源与本地发布目录内容一致，日志保留原始字节。

网页源码位于 `site/`，展示配置命令、采集流程、验证结果及已公开的原始日志。`scripts/build_site.py` 仅打包网页静态文件和归档日志到忽略的 `build-pages/`，不发布 SDK、构建缓存或相机程序。

## 首次开启

1. 打开 [仓库 Settings → Pages](https://github.com/chaorenqiang119/camera/settings/pages)。
2. 在 **Build and deployment → Source** 中选择 **GitHub Actions**。
3. 在 [Deploy project website](https://github.com/chaorenqiang119/camera/actions/workflows/pages.yml) 页面运行 **Run workflow**；若已有首次部署失败记录，可使用 **Re-run failed jobs**。
4. 等待 Publish website 完成，打开上述地址。后续修改 `site/`、网站构建脚本或归档日志并推送到 main 时会自动重新部署。

如 Configure GitHub Pages 步骤提示找不到 Pages site，先完成第 2 步。普通工作流的 GITHUB_TOKEN 可以部署已启用的 Pages，但无法代替管理员完成首次启用；本项目不要求把个人访问令牌放入源码。

## 本地预览

```bash
python scripts/build_site.py
python -m http.server 8765 --directory build-pages
```

打开 <http://localhost:8765/>。页面仅展示项目资料，相机采集需要在连接相机的本地电脑上运行。
