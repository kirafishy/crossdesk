# README 与使用指南截图 / Screenshot notes

[中文 README](../../README.md) · [English README](../../README_EN.md) · [中文使用指南](../USER_GUIDE.md) · [English user guide](../USER_GUIDE_EN.md)

## 来源 / Sources

- 16 张中英文桌面截图更新于 **2026-10-19**，UI 源代码为 `1ef372d2`，使用 Slint 1.17.1、macOS arm64 和软件渲染器。图片直接渲染仓库中的 Slint 组件。
- 捕获程序来自现有 [`slint_ui_smoke_test`](../../apps/desktop/tests/slint_ui_smoke_test.cpp)，在**工程外的临时目录**中复制源文件，仅为桌面墙调整演示设备数量、单元格宽度和网格窗口高度。Slint 组件没有改动，工程内没有新增或修改测试文件。
- 主窗口展示桌面墙入口、连接区、近期设备卡片与分组筛选；设置展示“常规 / 桌面墙”选项卡，关于窗口展示三个赞助入口。桌面墙网格展示六台演示设备；放大图展示只读预览、群控和接管入口，没有建立真实连接。
- 本机 ID `123 456 789`、密码 `123456`、设备名称、在线 / 连接状态及记录均为演示数据。主窗口展示三台设备，桌面墙展示六台；它们不是真实设备凭据或真实远程会话。示例配置不代表所有运行时默认值。
- “关于”中的 `v1.5.2` 和“已是最新版本”是捕获工具的示例值，不代表当前发布版本或实际在线检查结果。会话图使用公开页面作为远端画面，展示真实工具栏与画面设置菜单；隐私屏按钮处于关闭状态。
- 缩略图沿用既有公开页面截图：`fixtures/source.png` 来自 [CrossDesk Xmake 源码页](https://github.com/kunkundi/crossdesk/blob/ios-support/xmake.lua)，`fixtures/website.png` 来自 [CrossDesk 官网](https://www.crossdesk.cn/)，两者采集于 2026-09-07。它们只用于展示预览效果，不代表这些页面正在对应设备上运行，页面中的版本信息也不代表最新发布。
- `web-client.png` 保留 **2026-09-25** 从 [Web 客户端](https://web.crossdesk.cn/) 采集的 1280 × 720 连接首页，页面版本为 `2026.09.24.2`，ID 与密码为空。本次未重新采集 Web 页面；桌面设备预览继续复用这张图片。

All 16 desktop images were refreshed on **2026-10-19** from UI source `1ef372d2`, using Slint 1.17.1 on macOS arm64. They render the actual components with demonstration data. The capture program is an external temporary copy of the existing source, adjusted only for DeskHub's six sample devices, 300 logical-pixel cells, and shorter grid window; no UI components or repository tests were changed. The About view includes all three sponsorship links. IDs, credentials, connection states, versions, and update results are illustrative. No remote sessions were opened. Public-page fixtures remain from 2026-09-07, and the Web screenshot remains from 2026-09-25. This refresh does not include native mobile screenshots.

## 文件 / Files

| 文件（`zh` / `en`） | 页面 | PNG 尺寸 |
| --- | --- | --- |
| `desktop-main-*.png` | 主窗口 / Main window | 1280 × 944 |
| `desktop-deskhub-*.png`（不含 `focus`） | 六设备桌面墙 / Six-device DeskHub grid | 1920 × 1000 |
| `desktop-deskhub-focus-*.png` | 桌面墙放大视图 / Enlarged DeskHub view | 1920 × 1396 |
| `desktop-settings-*.png`（不含 `deskhub`） | 常规设置 / General settings | 1280 × 944 |
| `desktop-settings-deskhub-*.png` | 桌面墙传输设置 / DeskHub video settings | 1280 × 944 |
| `desktop-self-hosted-*.png` | 自托管设置 / Self-hosting | 1280 × 944 |
| `desktop-stream-video-settings-*.png` | 会话工具栏与画面设置 / Session video settings | 1920 × 1200 |
| `desktop-about-*.png` | 关于与检查更新 / About and update check | 1280 × 944 |
| `web-client.png`（中文） | Web 连接首页 / Web connection page | 1280 × 720 |

桌面图片使用 2× 原生渲染缩放，捕获窗口内容区，不包含 macOS 原生标题栏。未放大旧 UI 截图，也未重绘控件；演示远端画面由实际组件按比例显示。Markdown 只设置展示宽度。常规设置可滚动，截图仅展示首屏；Windows 专属采集方式与便携版服务入口不会出现在这些 macOS 图片中。

## 重新生成 / Regenerate

以下命令在仓库根目录、已配置依赖的 macOS 开发环境执行。所有临时源码、PPM 和额外捕获程序均写入系统临时目录，最终 PNG 才写入 `docs/images`。`sips` 仅做 PPM 到 PNG 的无损格式转换，其他平台可使用支持 PPM 的转换工具。

先检查 `/tmp/crossdesk-ui-capture-page` 和 `/tmp/crossdesk-ui-capture-language`：旧覆盖文件存在且非空时会优先于环境变量，应确认并移开后再捕获。`CROSSDESK_UI_CAPTURE_DEMO_ASSETS` 必须包含上述公开页面图片，缺失图片会导致捕获失败。工具等待窗口尺寸与缩放初始化后再截图。

### 主窗口、设置与会话

```bash
set -e
xmake b -y slint_ui_smoke_test
capture_dir=$(mktemp -d "${TMPDIR:-/tmp}/crossdesk-readme-capture.XXXXXX")
export capture_dir

for locale in zh en; do
  language=0
  if [ "$locale" = en ]; then language=1; fi
  for page in main settings settings-deskhub self-hosted about stream-video-settings; do
    snapshot="$capture_dir/desktop-$page-$locale.ppm"
    SLINT_BACKEND=winit-software SLINT_SCALE_FACTOR=2 \
      CROSSDESK_UI_CAPTURE=1 \
      CROSSDESK_UI_CAPTURE_DEMO_ASSETS="$PWD/docs/images" \
      CROSSDESK_UI_CAPTURE_PAGE="$page" \
      CROSSDESK_UI_CAPTURE_LANGUAGE="$language" \
      CROSSDESK_UI_CAPTURE_SNAPSHOT="$snapshot" \
      xmake r slint_ui_smoke_test
    sips -s format png "$snapshot" \
      --out "docs/images/desktop-$page-$locale.png"
  done
done
```

### 六设备桌面墙

现有捕获模式的 `gallery` / `gallery-focus` 默认使用三台设备。要复现本页的六设备图，在上一步同一 shell 中创建临时副本，只修改演示场景：

```bash
python3 - <<'PY'
import os
from pathlib import Path

source = Path('apps/desktop/tests/slint_ui_smoke_test.cpp').read_text()
changes = [
    ('options.page != "main-recent-scrolled") {',
     'options.page != "main-recent-scrolled" &&\n'
     '        !options.page.starts_with("gallery")) {'),
    ('window->set_gallery_mode(true);\n    std::vector',
     'window->set_gallery_mode(true);\n'
     '    window->set_gallery_tile_width(300);\n    std::vector'),
    ('698.0f;\n#else\n        720.0f;',
     'options.page == "gallery" ? 500.0f : 698.0f;\n#else\n'
     '        options.page == "gallery" ? 500.0f : 720.0f;'),
]
for old, new in changes:
    assert source.count(old) == 1, 'Capture source changed; review the fixture adjustment.'
    source = source.replace(old, new, 1)
Path(os.environ['capture_dir'], 'readme_capture.cpp').write_text(source)
PY
```

以下编译路径对应本次的 `macosx / arm64 / debug` 配置。其他配置先用 `xmake show -t slint_ui_smoke_test` 查看当前生成头文件和库目录，再替换路径；不要在工程内添加临时构建入口。

```bash
slint_prefix=$(xmake lua -c 'import("core.project.config"); config.load(); import("core.project.project"); print(project.target("slint_ui_smoke_test"):pkg("slint"):installdir())')
xcrun clang++ -std=c++20 \
  -Iapps/desktop/src/gui \
  -Iapps/desktop/src/gui/assets/fonts \
  -Iapps/desktop/src/gui/assets/localization \
  -Ibuild/.gens/crossdesk_ui/macosx/arm64/debug/rules/slint \
  -isystem "$slint_prefix/include/slint" \
  "$capture_dir/readme_capture.cpp" \
  build/macosx/arm64/debug/libcrossdesk_ui.a \
  -L"$slint_prefix/lib" -lslint_cpp -Wl,-rpath,"$slint_prefix/lib" \
  -o "$capture_dir/readme_capture"

for locale in zh en; do
  language=0
  if [ "$locale" = en ]; then language=1; fi
  for page in gallery gallery-focus; do
    name=deskhub
    if [ "$page" = gallery-focus ]; then name=deskhub-focus; fi
    snapshot="$capture_dir/desktop-$name-$locale.ppm"
    SLINT_BACKEND=winit-software SLINT_SCALE_FACTOR=2 \
      CROSSDESK_UI_CAPTURE=1 \
      CROSSDESK_UI_CAPTURE_DEMO_ASSETS="$PWD/docs/images" \
      CROSSDESK_UI_CAPTURE_PAGE="$page" \
      CROSSDESK_UI_CAPTURE_LANGUAGE="$language" \
      CROSSDESK_UI_CAPTURE_SNAPSHOT="$snapshot" \
      "$capture_dir/readme_capture"
    sips -s format png "$snapshot" \
      --out "docs/images/desktop-$name-$locale.png"
  done
done
```

更新后逐张检查文字、布局、演示数据和文档引用，并同步日期、源码版本与图片尺寸。若更新 Web 图片，应打开 Web 客户端，等待表单与底部版本号加载完成，保持 ID 与密码为空，再截取页面视口；替换后也需重新生成桌面图以同步缩略图。
