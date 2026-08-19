<div align="center">
  <img src="./QuickSay/icons/软件图标.svg" alt="[图片不见了...]" width="150">
  <h1>QuickSay</h1>
  <p>一款强大的快捷短语软件</p>
  <a href="https://github.com/DarkKandaoMaster/QuickSay/releases/latest"><img src="https://img.shields.io/github/v/release/DarkKandaoMaster/QuickSay?style=flat-square"></a>
  <a href="https://github.com/DarkKandaoMaster/QuickSay/stargazers"><img src="https://img.shields.io/github/stars/DarkKandaoMaster/QuickSay?style=flat-square&logo=github&color=yellow"></a>
  <a href="https://github.com/DarkKandaoMaster/QuickSay/releases"><img src="https://img.shields.io/github/downloads/DarkKandaoMaster/QuickSay/total?style=flat-square"></a><br>
  <a href="https://github.com/DarkKandaoMaster"><img src="https://img.shields.io/badge/Create_by_DarkKandaoMaster-with_Love_%E2%9D%A4-pink?style=flat-square"></a>
</div>

---

### 欢迎来测~ヾ(≧▽≦*)o
###### 该软件目前只支持Windows 10/11，不打算支持macOS/Linux/Windows 7


## QuickSay是干什么的？
如果您经常把常用短语（命令行、<strong title="输入Prompt真的很好用">AI提示词</strong>、固定的话术）保存在一个文本文件里，并在需要时打开这个文件复制粘贴。  
那么您就可以试用一下QuickSay。把常用短语保存在QuickSay里，并在需要时使用QuickSay输入。  

QuickSay的优势：  
- UI好看。简洁、现代、美观、高效。  
- 功能全面，比如支持按下角标对应按键输入、自定义短语快捷键输入、连续输入、搜索框搜索、备注等功能。  
- 甚至支持在短语中插入标签，让QuickSay除了输入文字，还能按键、停顿、粘贴图片/文件。  
- 免费、开源、无广、无需联网，未来我也不打算加上这些内容。  
- 开发者持续维护，愿意为它付出时间。  
- 因为是用Qt开发的，所以体积小（15MB）、运行快。  
- 适合Qt新手自学。代码注释超级详细，代码内容也比较简单，完全不用像学算法那样理解半天都理解不了这段代码是干什么的。如果把这代码给曾经的我看，估计用不了一周我就能自学成才，并且发布QuickSay_v1.0.0了。  

这里放一张软件窗口的全家福↓  
<img src="./README_Pictures/软件窗口的全家福.png" alt="[图片不见了...]"><br>


## 如何下载和安装
#### 方法一：
看到右边的Releases（发行版）了吗？点它，于是进入项目发布页，再点击链接QuickSay_v1.7.0.7z，于是开始下载。  
下载完成后，将得到的文件解压，得到：  
```
QuickSay文件夹
1_如何安装或更新QuickSay.txt
2_QuickSay使用操作.txt
```
直接把 **QuickSay文件夹** 放在随便什么文件夹里，然后双击QuickSay.exe。  
路径有中文空格都行，但是不要放在Program Files之类的需要管理员权限的文件夹里。  
#### 方法二：
点进这个蓝奏云链接：https://wwlt.lanzoum.com/b014wmlo5g 密码:star  
然后点击链接QuickSay_v1.7.0.7z，再点下载按钮，于是开始下载。  
接下来的步骤同上。  


## 如何更新
直接用 QuickSay文件夹 **替换**掉 电脑里原有的QuickSay文件夹 。  


## 基本使用
快捷键：按下快捷键（默认Ctrl+Shift+V）呼出QuickSay  
添加短语：右上角加号  
修改/删除：右键短语  
排序：拖动短语  

点击短语：输入对应短语  
上下方向键↑↓：移动光标  
回车键Enter：输入光标处短语  

右键左上角分组：新建/修改/删除分组  
分组排序：拖动分组  
左右方向键←→：切换分组  
鼠标滚轮：也可以切换分组  


## 常见使用问题
- 不要直接在解压软件里运行QuickSay.exe  
- 修改短语或设置后，退出重进QuickSay，发现短语根本没被修改，那必然是因为您把QuickSay安装在Program Files之类的需要管理员权限的文件夹里  
× 解决方法：把QuickSay安装在其他文件夹  


## 许可证
本项目采用 MIT 许可证。详情见 `LICENSE` 文件。也就是说您可以自由使用我的软件和代码，甚至商用。只要保留许可证和版权声明就行  


## 如何复现？如何运行代码？
1. 首先下载代码  
2. 然后您需要去下载个QT。教程推荐这篇文章：https://blog.csdn.net/qq_62888264/article/details/132645054  
3. 打开Qt Creator，按教程说的新建一个项目，名称写“QuickSay”，然后无脑下一步就行了  
4. 在文件资源管理器里找到这个新建的项目，把第1步下载下来的 QuickSay文件夹里的东西 直接**移动**到这个项目文件夹里，选“替换目标中的文件”  
5. 回到Qt Creator，点击左侧的“项目”，关闭“Shadow build：”这个选项  
6. 于是就可以运行了！**与我一同，成为开发者吧！**  
##### 说几个容易踩的坑：
- 如果运行出来一个窗口，但是窗口右上角没有齿轮图标之类的，那就是第4步没有把icons文件夹移动进去  
- 如果运行时报了一堆错，那大概率就是第5步没有关闭“Shadow build：”这个选项  


## 目前想到的之后版本要更新的内容
- 很多内容。因为想更新的内容太多了所以就暂时不写在这了  


## 目前不打算更新的内容
- 账号登录、同步  


# 感谢使用QuickSay！如果觉得好用记得点个Star！砍刀会感谢您的！
如有反馈或建议，欢迎直接在GitHub Issues中提交。因为这样我会收到邮件通知，肯定能及时回复。  
<img src="https://api.star-history.com/svg?repos=DarkKandaoMaster/QuickSay&type=Date" alt="[图片不见了...]"><br>


预计QuickSay会在八月底~九月初更新。大家等等我哈。最近我要做的事非常非常多...