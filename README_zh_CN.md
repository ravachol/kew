<div align="center">
  <img src="images/logo.png" alt="kew Logo">
</div>

<br>

<div align="center">
  <a href="https://jenova7.bandcamp.com/album/lost-sci-fi-movie-themes">
    <img src="images/kew.gif" alt="Screenshot">
  </a>
</div>
<br><br>

[![License](https://img.shields.io/github/license/ravachol/kew?color=333333&style=for-the-badge)](./LICENSE)
<br>
[English](README.md) | **简体中文**


kew (/kjuː/) 是一个终端音乐播放器。

## 功能

* 从命令行通过部分标题搜索音乐库。
* 根据匹配的歌曲、专辑或艺术家自动创建播放列表。
* 私密，无数据收集。
* 无算法推荐或干扰，纯粹的音乐体验。
* 在支持 sixel 的终端中显示全彩专辑封面。
* 可调节的可视化效果（Visualizer）。
* 编辑播放列表：添加、删除或重新排序歌曲。
* 无缝播放（Gapless Playback）。
* 浏览音乐库并将文件或文件夹加入队列。
* 支持从音乐库中搜索并添加到播放队列。
* 支持 MP3、FLAC、MPEG-4/M4A (AAC)、OPUS、OGG、Webm 和 WAV 格式。
* 通过 MPRIS 支持桌面事件。
* 通过 .lrc 文件、内嵌 SYLT（Mp3）或 Vorbis 注释（Flac, Ogg, Opus）支持歌词。
* 支持主题和从专辑封面提取颜色的配色方案。

## 安装

<a href="https://repology.org/project/kew/versions"><img src="https://repology.org/badge/vertical-allrepos/kew.svg" alt="Packaging status" align="right"></a>

可以通过包管理器或 Homebrew（macOS）安装。
如果你的发行版中还没有，或想获取最新版本，请参阅 [手动安装说明](docs/MANUAL-INSTALL-INSTRUCTIONS.md).

## 使用方法

`kew` 会根据命令行参数中提供的第一个匹配的目录或文件名创建播放列表并开始播放。

```bash
kew cure great
```

例如：如果你的音乐库中包含 “The cure greatest hits”，此命令会自动找到并播放该专辑。

建议将音乐库组织为以下结构：

艺术家文件夹 -> 专辑文件夹 -> 音轨文件

### 示例命令

```
kew                            无参数启动 kew，会打开音乐库视图以选择播放内容

kew all                        随机播放音乐库中所有歌曲，最多 20000 首

kew albums                     随机播放所有专辑，最多 2000 张

kew moonlight son              查找并播放《moonlight sonata》

kew beet                       查找并播放 “beethoven” 目录下的所有音乐

kew dir <专辑名>               当名称冲突时，可指定为目录

kew song <歌曲名>              或是歌曲

kew list <播放列表名>          或播放列表

kew theme midnight             设置 “midnight.theme” 主题

kew shuffle <专辑名>           随机播放，shuffle 参数需放在最前

kew 艺术家A:艺术家B:艺术家C    随机播放这三位艺术家的歌曲

kew --help, -? 或 -h           显示帮助

kew --version 或 -v            显示版本号

kew --nocover                  隐藏专辑封面

kew --noui                     完全隐藏界面

kew -q <歌曲>, --quitonstop    播放完毕后自动退出

kew -e <歌曲>, --exact         精确匹配专辑或歌曲名，不区分大小写

kew .                          加载 kew favorites.m3u

kew path "/home/joe/Musik/"    更改音乐库路径
```

### 按键绑定

#### 基本操作

* <kbd>Enter</kbd> 播放或加入/移出队列
* <kbd>Space</kbd>、<kbd>p</kbd> 或鼠标右键 播放/暂停
* <kbd>+</kbd>（或 <kbd>=</kbd>）、<kbd>-</kbd> 调节音量
* <kbd>←</kbd>、<kbd>→</kbd> 或 <kbd>h</kbd>、<kbd>l</kbd> 切换曲目
* <kbd>Alt+s</kbd> 停止播放
* <kbd>F2</kbd> 或 <kbd>Shift+z</kbd>（macOS/Android） 显示/隐藏播放列表视图
* <kbd>F3</kbd> 或 <kbd>Shift+x</kbd> 显示/隐藏音乐库视图
* <kbd>F4</kbd> 或 <kbd>Shift+c</kbd> 显示/隐藏曲目视图
* <kbd>F5</kbd> 或 <kbd>Shift+v</kbd> 显示/隐藏搜索视图
* <kbd>F6</kbd> 或 <kbd>Shift+b</kbd> 显示/隐藏快捷键视图
* <kbd>i</kbd> 在 kewrc、主题或封面之间切换颜色
* <kbd>t</kbd> 切换主题
* <kbd>Backspace</kbd> 清空播放列表
* <kbd>Delete</kbd> 删除单个播放列表条目
* <kbd>r</kbd> 切换循环模式（单曲、列表、关闭）
* <kbd>s</kbd> 随机播放

#### 高级操作

* <kbd>u</kbd> 更新音乐库
* <kbd>m</kbd> 在曲目视图中显示整页歌词 参见[歌词](#歌词)
* <kbd>v</kbd> 开关可视化效果
* <kbd>b</kbd> 在 ASCII 与正常封面之间切换
* <kbd>n</kbd> 开关通知
* <kbd>a</kbd> 快退
* <kbd>d</kbd> 快进
* <kbd>x</kbd> 将当前播放列表保存为 .m3u 文件
* <kbd>Tab</kbd> 切换至下一个视图
* <kbd>Shift+Tab</kbd> 切换至上一个视图
* <kbd>e</kbd> 在文件管理器中打开当前歌曲所在文件夹
* <kbd>f</kbd>、<kbd>g</kbd> 移动歌曲在播放列表中的位置
* 数字 + <kbd>G</kbd> 或 <kbd>Enter</kbd> 跳转至指定歌曲编号
* <kbd>.</kbd> 将当前播放歌曲添加至 kew favorites.m3u（运行 `kew .`）
* <kbd>Esc</kbd> 退出程序

## 配置文件位置

Linux: ~/.config/kew/

macOS: ~/Library/Preferences/kew/

## 主题

按 <kbd>t</kbd> 切换可用主题。
也可通过命令行设置主题：

```bash
kew theme <主题名> （例如：kew theme midnight）
```

将主题文件放置在目录 \~/.config/kew/themes (macOS 为 \~/Library/Preferences/kew/themes).

## 如果颜色或图形显示异常

按 <kbd>i</kbd> 切换颜色模式，直到显示正常。

按 <kbd>v</kbd> 关闭可视化效果。

按 <kbd>b</kbd> 切换为 ASCII 封面显示。

建议使用支持 TrueColor 和 Sixel 的终端模拟器。[Sixels in Terminal](https://www.arewesixelyet.com/).

## 歌词

kew 可读取与音频文件同名的 .lrc 文件，或从 mp3 的 SYLT 标签、Flac/Ogg/Opus 的 Vorbis 注释中提取歌词。
带时间戳的歌词会在曲目视图中自动显示。
按 <kbd>m</kbd> 可查看整页歌词。

## 播放列表

加载播放列表: kew list <名称>

导出播放列表, 按 `x`，这会保存播放列表至你的音乐库目录下，文件名为当前队列首首歌曲。

也有“我喜欢”功能:

添加当前歌曲到“我喜欢”播放列表：按 <kbd>.</kbd>

加载“我喜欢”播放列表：

```bash
kew .
```

## 许可证

本项目基于 GPL 协议开源。
详细内容请参阅 [LICENSE](./LICENSE).


## 致谢

<details>
<summary>Attributions</summary>

kew 使用了以下优秀的开源项目:

Chafa，由 Hans Petter Jansson 开发 - https://hpjansson.org/chafa/

TagLib，由 TagLib 团队开发 - https://taglib.org/

Faad2，由 fabian_deb、knik、menno 开发 - https://sourceforge.net/projects/faac/

FFTW，由 Matteo Frigo 和 Steven G. Johnson 开发 - https://www.fftw.org/

Libopus，由 Opus 团队开发 - https://opus-codec.org/

Libvorbis，由 Xiph.org 开发 - https://xiph.org/

Miniaudio，由 David Reid 开发 - https://github.com/mackron/miniaudio

Minimp4，由 Lieff 开发 - https://github.com/lieff/minimp4

Nestegg，由 Mozilla 开发 - https://github.com/mozilla/nestegg

Img_To_Txt，由 Danny Burrows 开发 - https://github.com/danny-burrows/img_to_txt

</details>

## 作者

参见 [AUTHORS](./docs/AUTHORS.md).

## 联系方式

反馈与建议请发送邮件至: kew-player@proton.me.
