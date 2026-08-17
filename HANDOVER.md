# StreamPlayer 交接记录（待处理）

更新时间：2026-08-17

## 1) 启动/退出时强制处理 APPLaunch（先杀后恢复）

需求：StreamPlayer 独占启动时，当前实例应先停止 APPLaunch，退出前再拉起 APPLaunch。

目前观察到的行为
- 应用初始化时已经做了 `PackageKit` 与 `display` 守护启动，但没有显式停止 APPLaunch。
- `shutdown()` 只做 `restore_display_performance_mode()` / `restore_background_performance_mode()`，没有反向恢复 APPLaunch。
- 已确认：如果单独手动启动 StreamPlayer，它会与 APPLaunch 并存，可能造成交互/焦点和显示状态竞争。

建议落地项
- 在 StreamPlayer 生命周期内增加独立开关位 `applaunch_stopped_by_streamplayer_`。
- 启动点（`MediaController::init()` 或首次播放前）：
  - 检查 `APPLaunch.service` 是否在运行（用户态 +/或 root 视部署方式）。
  - 记录原始状态；
  - 尝试停止 APPLaunch（失败时记录错误，不阻塞到无法启动）。
- 退出点（`MediaController::shutdown()`）：
  - 如果 `applaunch_stopped_by_streamplayer_ == true`，在资源 guard 已经恢复逻辑后再启动 APPLaunch。
  - 加上超时+重试，避免启动失败拖死主流程。
- 增加环境变量开关（仅用于排障）：
  - `M5_STREAMPLAYER_KEEP_APPLAUNCH=1`（不自动 stop/start）。

## 2) 海报（封面/背景图）不显示 — 已修复

2026-08-17 已落地，版本 `0.1.0-m5stack15`：
- 海报缓存改为稳定目录 `/home/pi/.cache/m5streamplayer/posters`（可用 `M5_STREAMPLAYER_CACHE_DIR` 覆盖），不再依赖 APPLaunch 的 `/usr/share/APPLaunch` cwd。
- 下载失败、空文件、非 PNG、转码失败都会写 `/tmp/m5streamplayer-app.log`。
- 剧集不再因为横竖判断失败就 `unlink` 已下载图片；没有横图时回退到任意已下载图，再回退到系列海报。
- 服务器如果返回 JPEG/WebP，会尝试用 `ffmpeg` 转成 PNG 再交给 LVGL。
- 已交叉编译：`dist/M5CardputerZero-StreamPlayer`
- 已打包：`artifacts/streamplayer_0.1.0-m5stack15_arm64.deb`
- 2026-08-17 部署未完成：设备 `192.168.199.179` ping/SSH 超时。设备上线后执行 `CARDPUTER_ZERO_SSH_PASSWORD=pi ./deploy_cardputerzero.sh 192.168.199.179 pi`

复测：
- `ls -l /home/pi/.cache/m5streamplayer/posters`
- `tail -n 50 /tmp/m5streamplayer-app.log`
- 启动日志应包含 `version=0.1.0-m5stack15` 和 `poster_cache=/home/pi/.cache/m5streamplayer/posters`

## 3) 最近播放（历史）海报缓存能力核查

结论：
- 代码里已有通用海报缓存：`cached_image_path()` / `cached_image_fit_path()` 会将图片下载到 `/home/pi/.cache/m5streamplayer/posters`，文件名按 `item.id + kind + 规格` 持久化。
- `最近播放`（最近库）目前不是独立缓存逻辑，而是通过 `load_items()` 里对 `kRecentPlayedLibraryId` 的查询实现：`SortBy=DatePlayed&SortOrder=Descending&Filters=IsPlayed`。
- 也就是说它会复用同一套海报缓存，不是“单独历史海报缓存模块”。

建议（待确认/是否需要）：
- 若目标是“历史页开机秒开且避免重复请求”，新增一条“最近播放历史海报预热（prefetch）”步骤，把最近播放列表前 N 条在进入页面前提前下载，独立于用户滚动时按需加载。
- 历史页和主列表已共用同一份稳定缓存目录；如需开机秒开，再补最近播放前 N 条预热即可。

## 4) 长启动缓冲“载入中”反馈（Small Screen Play）

需求：按下 `play on small screen` 后，视频进入播放前有较长等待时，界面先显示“载入中，请耐心等待”。

建议落地项（不改代码，仅记录）：
- 在触发播放动作的入口（例如按键映射里 `play on small screen` 分支）先显示一个临时全局状态层（overlay / toast）。
- 文案建议：`载入中，请耐心等待...`，并允许本地化。
- 状态层应在以下时机移除：
  - 收到首帧可显示时；
  - 或播放失败/超时退出时（显示失败提示）。
- 避免在 overlay 显示期间阻塞输入事件；用户可继续看到界面过渡（允许出现退回入口或取消键）。
- 加超时保护：若超过 N 秒未进入 PLAYING（例如 12-20 秒），显示“网络波动，请稍候后重试”。
- 上报计时指标到日志：按键时间、缓冲开始时间、首帧时间、超时重试次数，便于判断卡在“网络/鉴权/解码”哪一段。

## 5) 退出确认窗口（长按/连按 Esc 防误退）

需求：长按 `Esc` 或连续快速按 `Esc` 时，避免直接退出 StreamPlayer，给用户确认入口。

建议落地项（不改代码，仅记录）：
- 触发策略（择一或二者并存）：
  - 长按 `Esc`（例如按下持续 `>=700ms`）；
  - 短时间窗口（例如 800ms 内）连续按 `Esc` 达到 2~3 次。
- 弹窗文案示例：
  - 标题：`确认退出`
  - 内容：`是否退出 StreamPlayer？`
  - 按钮：`确认` / `取消`
- 用户体验要求：
  - 仅在用户确认后退出；
  - 取消后返回上次焦点/当前播放页；
  - 显示确认窗期间，继续保留 `Escape` 的二次确认语义（避免重复误触）。
- 安全兜底：
  - 在播放关键失败恢复/错误弹窗等场景，允许保留原有快速退出路径。
- 日志建议：
  - 记录触发类型（长按/连按）、触发次数、停留时长、是否确认、是否取消，便于后续判定误退率。
