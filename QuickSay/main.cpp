//版本：1.6.0
//更新内容：
//1. 修改了一下默认短语
//2. 实现了无焦点主窗口。主窗口不会再抢焦点了
//3. 短语文本可以支持某些特定规则，于是实现更高级、更复杂地输入。

#include<QApplication>
#include<QWidget>
#include<QListWidget>
#include<QLabel>
#include<QClipboard>
#include<QSystemTrayIcon>
#include<QIcon>
#include<QMenu>
#include<QHotkey>
#include<QPushButton>
#include<QPlainTextEdit>
#include<QJsonArray>
#include<QJsonObject>
#include<QJsonDocument>
#include<QFile>
#include<QKeyEvent>
#include<QFormLayout>
#include<QKeySequenceEdit>
#include<QHBoxLayout>
#include<QSpinBox>
#include<QLineEdit>
#include<QMessageBox>
#include<QCheckBox>
#include<QSettings>
#include<QFileInfo>
#include<QDir>
#include<SingleApplication.h>
#include<QTimer>
#include<QVector>
#include<QTabBar>
#include<QWheelEvent>
#include<QMouseEvent>
#include<QInputDialog>
#include<QStyledItemDelegate>
#include<QPainter>
#include<QScrollBar>
#include<QDialog>
#include<QDesktopServices>
#include<QUrl>
#include<QAbstractNativeEventFilter>
#include<QImage>
#include<QStandardPaths>
#include<QToolTip>
#include<QUuid>
#include<climits>
#include<windows.h>
#pragma comment(lib,"user32.lib")

QJsonObject config;//全局对象，用于保存程序的设置

QWidget * pchuangkou=nullptr;
QWidget * g_shezhichuangkou=nullptr;
QWidget * g_tianjiachuangkou=nullptr;
QWidget * g_xiugaichuangkou=nullptr;
QListWidget * g_liebiao=nullptr;
QTabBar * g_tabBar=nullptr;
QLineEdit * g_search=nullptr;
HHOOK g_keyboardHook=nullptr;
int g_quickSayPressBlockCount=0;
bool g_quickSayIsOutputting=false;
bool g_searchMode=false;
HWND g_lastForegroundBeforeSearch=nullptr;

void saveConfig(const QString & configPath){ //写入程序设置到config.json
    QJsonDocument doc(config);//把全局对象config转换成JSON文档
    QFile file(configPath);//打开指定路径文件config.json
    if(file.open( QIODevice::WriteOnly | QIODevice::Text )){ //如果config.json成功打开（写模式）
        file.write(doc.toJson());//写入JSON文档到本地
        file.close();//关闭文件
    }
}

void loadConfig(const QString & configPath){ //读取config.json到程序设置。如果config.json不存在，那么读取默认设置，同时写入默认设置到config.json
    QFile file(configPath);
    if(file.open( QIODevice::ReadOnly | QIODevice::Text )){ //如果config.json成功打开（读模式）
        QByteArray data=file.readAll();//把config.json所有内容读取到data
        file.close();//关闭文件
        QJsonDocument doc=QJsonDocument::fromJson(data);//把读取到的数据转换成JSON文档
        if(doc.isObject()){ //确认文档是一个对象
            config=doc.object();//取出JSON对象，把这个对象赋值给全局对象config
            //兼容旧版本
            if(!config.contains("delay")) config["delay"]=100;//如果config里没有delay，那么默认延迟多少毫秒再输入？100毫秒
            if(!config.contains("default_item_height")) config["default_item_height"]=40;//如果config里没有default_item_height，那么默认短语项高度40
            if(!config.contains("gundong")) config["gundong"]=10;//如果config里没有gundong，那么默认滚动条滚动速度10
        }
    }
    else{ //如果config.json不存在
        config["hotkey"]="Ctrl+Shift+V";//默认全局快捷键【【【注：想修改默认设置在这里修改】】】
        config["zhiding"]=true;//默认主窗口始终置顶
        config["clipboard"]=true;//默认输入时也将短语复制到剪贴板
        config["delay"]=100;//默认延迟多少毫秒再输入？100毫秒
        config["width"]=500;//默认窗口宽度
        config["height"]=500;//默认窗口高度
        config["default_item_height"]=40;//默认短语项高度40
        config["gundong"]=10;//默认滚动条滚动速度10
        config["jiaobiao"]=false;//默认角标放在右上角
        config["ziqidong"]=true;//默认开机自启动
        config["tudingflag"]=true;//默认钉住窗口
        config["chuangkou_x"]=( QGuiApplication::primaryScreen()->geometry().width()-500 )/2;//chuangkou默认显示位置 //获取屏幕的宽高，然后 (屏幕宽度-窗口宽度)/2 ，于是就获得了能让窗口在x轴上居中显示的位置
        config["chuangkou_y"]=( QGuiApplication::primaryScreen()->geometry().height()-500 )/2;
        config["shezhichuangkou_x"]=( QGuiApplication::primaryScreen()->geometry().width()-500 )/2+501;//shezhichuangkou默认显示位置。加上501是为了不让它和主窗口重叠
        config["shezhichuangkou_y"]=( QGuiApplication::primaryScreen()->geometry().height()-500 )/2;
        config["tianjiachuangkou_x"]=( QGuiApplication::primaryScreen()->geometry().width()-500 )/2+501;//tianjiachuangkou默认显示位置
        config["tianjiachuangkou_y"]=( QGuiApplication::primaryScreen()->geometry().height()-500 )/2;
        config["xiugaichuangkou_x"]=( QGuiApplication::primaryScreen()->geometry().width()-500 )/2+501;//xiugaichuangkou默认显示位置
        config["xiugaichuangkou_y"]=( QGuiApplication::primaryScreen()->geometry().height()-500 )/2;
        saveConfig(configPath);//写入默认设置到config.json
    }
}

void updateItemDisplay(QListWidgetItem * it){ //更新对应短语项的显示，如果对应的备注不是空字符串，那么显示备注；否则显示短语
    QString text=it->data(Qt::UserRole).toString();//取出短语
    QString remark=it->data(Qt::UserRole+1).toString();//取出备注
    it->setToolTip(   remark.isEmpty()?text:(remark+"\n────────────────────────\n"+text)   );//为短语项设置鼠标悬停时的提示文字。这样用户就可以通过鼠标悬停查看完整短语了
    if(!remark.isEmpty()){ //如果备注不是空字符串
        it->setText(remark);//设置显示文本为备注
        it->setForeground(QColor("#4A90E2"));//设置备注字体颜色：蓝色【【【注：想修改备注字体颜色在这里修改】】】
    }
    else{
        it->setText(text);//设置显示文本为短语
        it->setForeground(QColor("#323130"));//设置短语字体颜色：深灰色
    }
}

void saveListToJson(QListWidget & liebiao,const QString & dataPath){ //写入列表内容到data.json
    QJsonArray jsonArray;//创建一个JSON数组
    for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
        QJsonObject obj;//创建一个JSON对象
        if(liebiao.item(i)->data(Qt::UserRole).toString().isEmpty()){ //如果当前项的短语为空，那么说明当前项的Qt::UserRole还没设置
            liebiao.item(i)->setData(Qt::UserRole,   liebiao.item(i)->text()   );//把当前项的文本存到当前项的Qt::UserRole
        }
        if(liebiao.item(i)->data(Qt::UserRole+2).toString().isEmpty()){ //如果当前项的标签为空，那么说明当前项的Qt::UserRole+2还没设置
            liebiao.item(i)->setData(Qt::UserRole+2,"短语1");//把"短语1"存到当前项的Qt::UserRole+2
        }
        obj["text"]=liebiao.item(i)->data(Qt::UserRole).toString();//把当前项的短语存到"text"字段
        obj["remark"]=liebiao.item(i)->data(Qt::UserRole+1).toString();//把当前项的备注存到"remark"字段
        obj["tab"]=liebiao.item(i)->data(Qt::UserRole+2).toString();//把当前项的标签存到"tab"字段
        obj["hotkey"]=liebiao.item(i)->data(Qt::UserRole+3).toString();//把当前项的快捷键字符串存到"hotkey"字段，如果没有设置则为空字符串
        jsonArray.append(obj);//把对象加入数组
    }
    QJsonDocument doc(jsonArray);//把数组包装成JSON文档
    QFile file(dataPath);//打开指定路径的文件data.json
    if(file.open( QIODevice::WriteOnly | QIODevice::Text )){ //如果文件成功打开（写模式）
        file.write(doc.toJson());//把JSON文档写入文件
        file.close();//关闭文件
    }
}

void loadListFromJson(QListWidget & liebiao,const QString & dataPath){ //读取data.json到列表内容。如果data.json不存在，那么读取默认列表内容（也就是新手教程），同时写入默认列表内容到data.json
    QFile file(dataPath);//打开指定路径的文件data.json
    if(file.open( QIODevice::ReadOnly | QIODevice::Text )){ //如果文件成功打开（读模式）
        QByteArray data=file.readAll();//把文件内容读到内存
        file.close();//关闭文件
        QJsonDocument doc=QJsonDocument::fromJson(data);//把数据解析成JSON文档
        if(doc.isArray()){ //确认文档是一个数组
            liebiao.clear();//清空列表
            QJsonArray jsonArray=doc.array();//取出JSON数组
            for(auto value:jsonArray){ //遍历数组中的每个元素
                if(value.isObject()){ //确认元素是对象
                    QJsonObject obj=value.toObject();//转换为对象
                    QString text=obj["text"].toString();//取出"text"字段，即短语
                    QString remarkStr=obj["remark"].toString();//取出"remark"字段，即备注
                    QString tabStr=obj["tab"].toString();//取出"tab"字段，即标签
                    QString hotkeyStr=obj["hotkey"].toString();//取出"hotkey"字段，即短语快捷键
                    //兼容旧版本【【【【【我能不能把saveListToJson里的兼容旧版本的代码写在loadListFromJson里？
                    QListWidgetItem * it=new QListWidgetItem();//new一个短语项对象，并把它的地址给it。不直接liebiao.addItem(text)是因为这样就用不了Qt::UserRole了；使用new是因为这样它就不会在当前函数结束时被销毁
                    it->setData(Qt::UserRole,text);//把短语存到it的Qt::UserRole
                    it->setData(Qt::UserRole+1,remarkStr);//把备注存到it的Qt::UserRole+1
                    it->setData(Qt::UserRole+2,tabStr);//把标签存到it的Qt::UserRole+2
                    it->setData(Qt::UserRole+3,hotkeyStr);//把快捷键字符串存到it的Qt::UserRole+3
                    updateItemDisplay(it);//更新对应短语项的显示
                    liebiao.addItem(it);//把it添加到列表。没错直接addItem指针是完全可以的 //it的内存不用释放，因为此时liebiao会接管it的所有权，自动管理其内存
                }
            }
        }
    }
    else{ //如果data.json不存在
        auto addDefaultItem=[&](const QString & text){ //定义一个局部函数，用于添加默认短语
            QListWidgetItem * it=new QListWidgetItem();
            it->setData(Qt::UserRole,text);
            it->setData(Qt::UserRole+1,"");
            it->setData(Qt::UserRole+2,"短语1");
            it->setData(Qt::UserRole+3,"");
            updateItemDisplay(it);//更新对应短语项的显示
            liebiao.addItem(it);
        };
        addDefaultItem("快捷键：按下快捷键（默认Ctrl+Shift+V）呼出QuickSay\n添加短语：点右上角加号\n修改/删除：右键短语\n排序：拖动短语");//【【【注：想修改默认列表内容（也就是新手教程）在这里修改】】】
        addDefaultItem("点击短语：输入对应短语\n鼠标悬停：查看完整短语\n上下方向键↑↓：移动光标\n回车键Enter：输入光标处短语");
        addDefaultItem("右键标签：添加/修改/删除标签\n标签排序：拖动标签\n左右方向键←→：切换标签\n鼠标滚轮：也可以切换标签");
        addDefaultItem("感谢使用QuickSay！\n如果觉得好用记得去Github点个Star！\n让我们开始吧！把这些短语都删掉，然后新建一个短语~");
        saveListToJson(liebiao,dataPath);
    }
}

void saveTabToJson(QTabBar & tabBar,const QString & tabPath){ //写入标签栏内容到tab.json
    QJsonArray jsonArray;//创建一个JSON数组
    for(int i=0;i<tabBar.count();i++){ //遍历标签栏中的所有标签
        QJsonObject obj;//创建一个JSON对象
        obj["tab"]=tabBar.tabText(i);//把当前标签的文本存到"tab"字段
        obj["item_height"]=tabBar.tabData(i).toInt();//把当前标签的短语项高度存到"item_height"字段
        jsonArray.append(obj);//把对象加入数组
    }
    QJsonDocument doc(jsonArray);//把数组包装成JSON文档
    QFile file(tabPath);//打开指定路径的文件tab.json
    if(file.open( QIODevice::WriteOnly | QIODevice::Text )){ //如果文件成功打开（写模式）
        file.write(doc.toJson());//把JSON文档写入文件
        file.close();//关闭文件
    }
}

void loadTabFromJson(QTabBar & tabBar,const QString & tabPath){ //读取tab.json到标签栏内容。如果tab.json不存在，那么读取默认标签栏内容，同时写入默认标签栏内容到tab.json
    QFile file(tabPath);//打开指定路径的文件tab.json
    if(file.open( QIODevice::ReadOnly | QIODevice::Text )){ //如果文件成功打开（读模式）
        QByteArray data=file.readAll();//把文件内容读到内存
        file.close();//关闭文件
        QJsonDocument doc=QJsonDocument::fromJson(data);//把数据解析成JSON文档
        if(doc.isArray()){ //确认文档是一个数组
            while(tabBar.count()>0){ //清空标签栏。因为tabBar没有clear()所以我们只能这么做
                tabBar.removeTab(0);
            }
            QJsonArray jsonArray=doc.array();//取出JSON数组
            for(auto value:jsonArray){ //遍历数组中的每个元素
                if(value.isObject()){ //确认元素是对象
                    QJsonObject obj=value.toObject();//转换为对象
                    QString tab=obj["tab"].toString();//取出"tab"字段，即标签
                    int item_height=obj["item_height"].toInt();//取出"item_height"字段，即短语项高度
                    //兼容旧版本
                    if(item_height==0) item_height=config["default_item_height"].toInt();//如果"item_height"为0，那么使用默认短语项高度
                    tabBar.addTab(tab);//把标签添加到标签栏
                    tabBar.setTabData(tabBar.count()-1,item_height);//把短语项高度存到对应标签的tabData中。因为目前我们只需要保存一个“短语项高度”，所以为了图省事，我们直接把这个值塞进了tabData。如果以后要加字段，那么可以往tabData里塞一个字典
                }
            }
        }
    }
    else{ //如果tab.json不存在
        tabBar.addTab("短语1");//【【【注：想修改默认标签栏内容在这里修改】】】
        tabBar.setTabData(0,70);
        tabBar.addTab("短语2");
        tabBar.setTabData(1,config["default_item_height"].toInt());
        tabBar.addTab("右键可添加标签");
        tabBar.setTabData(2,config["default_item_height"].toInt());
        saveTabToJson(tabBar,tabPath);
    }
}

void filterListByTab(QListWidget & liebiao,const QString & currentTab,const QString & searchKeyword){ //根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
    int visibleCount=0;//用于计数当前遍历到的可见项
    for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
        QListWidgetItem * item=liebiao.item(i);
        bool isTextMatch=true;//判断搜索框文字是否匹配
        if(!searchKeyword.isEmpty()){ //如果搜索框里有字
            isTextMatch=(   item->data(Qt::UserRole).toString()   ).contains(searchKeyword,Qt::CaseInsensitive)   ||   (   item->data(Qt::UserRole+1).toString()   ).contains(searchKeyword,Qt::CaseInsensitive);//如果短语包含了搜索框文字，或者备注包含了搜索框文字（Qt::CaseInsensitive表示不区分大小写），那么返回true
        }
        if(   item->data(Qt::UserRole+2).toString()   ==currentTab   &&isTextMatch){ //如果该短语项的标签名称等于当前选中标签的名称，并且搜索框文字也匹配
            item->setHidden(false);//显示该短语项
            visibleCount++;
            QString badgeStr="";//用于临时存放角标字符
            if(visibleCount>=1 && visibleCount<=9){ //如果是第1~9条短语
                badgeStr=QString::number(visibleCount);//直接显示数字
            }
            else if(visibleCount==10){ //如果是第10条短语
                badgeStr="0";//显示0
            }
            else if(visibleCount>=11 && visibleCount<=36){ //如果是第11~36条短语
                badgeStr=QChar('A'+visibleCount-11);//显示A~Z
            }
            //如果超过36条那么badgeStr直接为空字符串，即不显示角标
            item->setData(Qt::UserRole+4,badgeStr);//把badgeStr存到该短语项的Qt::UserRole+4
        }
        else{
            item->setHidden(true);//隐藏该短语项
            item->setData(Qt::UserRole+4,"");//清除该短语项的Qt::UserRole+4
        }
    }
}

bool isTabNameDuplicate(QTabBar & tabBar,const QString & tabName){ //判断添加/修改标签时用户给出的标签名称是否重复
    for(int i=0;i<tabBar.count();i++){ //遍历标签栏中的所有标签
        if(   tabBar.tabText(i)   ==tabName) return true;//如果名称重复，那么返回true
    }
    return false;
}

bool showTabDialog(QWidget & parent,const QString & title,QString & tabName,int & itemHeight){ //用于弹出一个同时包含“标签名称”和“短语项高度”两个输入框的对话框【【【【【待处理
    QDialog dialog(&parent);//创建一个局部对话框对象，将传入的 parent 作为父窗口，对话框关闭时内存会自动回收
    dialog.setWindowTitle(title);//设置对话框的标题文字
    dialog.setFixedSize(280,130);//设置固定的窗口大小（宽280，高130），防止窗口被拉伸变形

    QFormLayout layout(&dialog);//创建一个表单布局，用于整齐地将标签文字和输入框左右排列

    QLineEdit nameEdit(&dialog);//创建一个单行文本输入框，用于让用户输入标签名称
    nameEdit.setText(tabName);//将初始的标签名称（如旧名称或空字符串）放入输入框中
    layout.addRow("标签名称：",&nameEdit);//把提示文字和名称输入框作为一行添加到表单布局中

    QSpinBox heightSpin(&dialog);//创建一个数字输入框，用于让用户输入短语项高度
    heightSpin.setRange(10,100);//设置数字输入框的可输入范围为10到100像素【【【【【
    heightSpin.setValue(itemHeight);//将初始的高度值（如默认高度或已有高度）放入输入框中
    layout.addRow("短语项高度：",&heightSpin);//把提示文字和高度输入框作为一行添加到表单布局中

    QHBoxLayout btnLayout;//创建一个水平布局，用于放置“确定”和“取消”按钮
    QPushButton btnCancel("取消",&dialog);//创建一个文本为“取消”的按钮
    QPushButton btnOk("确定",&dialog);//创建一个文本为“确定”的按钮
    btnLayout.addStretch();//在按钮左侧添加一个弹簧，把两个按钮挤到窗口的右下角，看起来更美观
    btnLayout.addWidget(&btnCancel);//把“取消”按钮放入水平布局
    btnLayout.addWidget(&btnOk);//把“确定”按钮放入水平布局
    layout.addRow(&btnLayout);//把包含了两个按钮的水平布局添加到整个表单布局的最后一行

    //当点击“确定”按钮时，触发对话框的 accept 槽函数（表示用户确认，准备关闭窗口）
    QObject::connect(&btnOk,&QPushButton::clicked,&dialog,&QDialog::accept);
    //当点击“取消”按钮时，触发对话框的 reject 槽函数（表示用户取消，准备关闭窗口）
    QObject::connect(&btnCancel,&QPushButton::clicked,&dialog,&QDialog::reject);

    if(dialog.exec()==QDialog::Accepted){ //弹出对话框并阻塞程序执行。如果用户点击了“确定”（返回值是 Accepted）
        tabName=nameEdit.text();//获取名称输入框里的文本，存入传入的 tabName 引用变量中
        itemHeight=heightSpin.value();//获取高度输入框里的数字，存入传入的 itemHeight 引用变量中
        return true;//返回 true，告诉调用者用户确认了输入
    }
    return false;//如果用户点击了“取消”或按右上角关闭了窗口，返回 false
}

void moniCtrlV(){ //模拟Ctrl+V
    INPUT inputs[4]={};//定义一个长度为4的数组，存放：左Ctrl键按下、V键按下、V键抬起、左Ctrl键抬起
    //左Ctrl键按下
    inputs[0].type=INPUT_KEYBOARD;
    inputs[0].ki.wVk=VK_LCONTROL;
    //V键按下
    inputs[1].type=INPUT_KEYBOARD;
    inputs[1].ki.wVk='V';
    //V键抬起
    inputs[2].type=INPUT_KEYBOARD;
    inputs[2].ki.wVk='V';
    inputs[2].ki.dwFlags=KEYEVENTF_KEYUP;
    //左Ctrl键抬起
    inputs[3].type=INPUT_KEYBOARD;
    inputs[3].ki.wVk=VK_LCONTROL;
    inputs[3].ki.dwFlags=KEYEVENTF_KEYUP;
    //一次性发送这4个事件
    SendInput(4,inputs,sizeof(INPUT));
}

struct QuickSayDropFiles{
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
};

enum class QuickSayOutputActionType{
    Text,
    Press,
    Sleep,
    Image
};

struct QuickSayOutputAction{
    QuickSayOutputActionType type=QuickSayOutputActionType::Text;
    QString text;
    QVector<WORD> modifiers;
    WORD key=0;
    int sleepMs=0;
    QImage image;
    QString imagePath;
};

bool setClipboardFileWin32(const QString & filePath){
    QFileInfo info(filePath);
    if(!info.exists() || !info.isFile()) return false;

    QString nativePath=QDir::toNativeSeparators(info.absoluteFilePath());
    int pathBytes=(nativePath.size()+2)*sizeof(wchar_t);
    SIZE_T totalSize=sizeof(QuickSayDropFiles)+pathBytes;
    HGLOBAL hMem=GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,totalSize);
    if(!hMem) return false;

    BYTE * ptr=(BYTE*)GlobalLock(hMem);
    if(!ptr){
        GlobalFree(hMem);
        return false;
    }

    QuickSayDropFiles * dropFiles=(QuickSayDropFiles*)ptr;
    dropFiles->pFiles=sizeof(QuickSayDropFiles);
    dropFiles->fWide=TRUE;
    memcpy(ptr+sizeof(QuickSayDropFiles),nativePath.utf16(),pathBytes-sizeof(wchar_t));
    GlobalUnlock(hMem);

    if(!OpenClipboard(nullptr)){
        GlobalFree(hMem);
        return false;
    }

    EmptyClipboard();
    bool ok=SetClipboardData(CF_HDROP,hMem)!=nullptr;
    CloseClipboard();
    if(!ok) GlobalFree(hMem);
    return ok;
}

QString saveImageToClipboardTempFile(const QImage & image){
    if(image.isNull()) return QString();

    QString dirPath=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if(dirPath.isEmpty()) dirPath=QDir::tempPath()+"/QuickSay";

    QDir dir(dirPath);
    if(!dir.exists() && !dir.mkpath(".")) return QString();

    QString path=dir.filePath("clipboard-image-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".png");
    if(!image.save(path,"PNG")) return QString();
    return path;
}

bool setClipboardImageFileWin32(const QuickSayOutputAction & action){
    if(!action.imagePath.isEmpty() && setClipboardFileWin32(action.imagePath)) return true;

    QString tempPath=saveImageToClipboardTempFile(action.image);
    if(tempPath.isEmpty()) return false;
    return setClipboardFileWin32(tempPath);
}

struct QuickSayModifierSnapshot{
    bool lCtrl=false;
    bool rCtrl=false;
    bool lAlt=false;
    bool rAlt=false;
    bool lShift=false;
    bool rShift=false;
    bool lMeta=false;
    bool rMeta=false;
};

bool isExtendedVirtualKey(WORD vk){
    switch(vk){
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

void sendVirtualKey(WORD vk,bool keyUp=false){
    INPUT input={};
    input.type=INPUT_KEYBOARD;
    input.ki.wVk=vk;
    if(keyUp) input.ki.dwFlags|=KEYEVENTF_KEYUP;
    if(isExtendedVirtualKey(vk)) input.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;
    SendInput(1,&input,sizeof(INPUT));
}

QuickSayModifierSnapshot releasePressedPhysicalModifiers(){
    QuickSayModifierSnapshot snapshot;
    snapshot.lCtrl=( GetAsyncKeyState(VK_LCONTROL) & 0x8000 )!=0;
    snapshot.rCtrl=( GetAsyncKeyState(VK_RCONTROL) & 0x8000 )!=0;
    snapshot.lAlt=( GetAsyncKeyState(VK_LMENU) & 0x8000 )!=0;
    snapshot.rAlt=( GetAsyncKeyState(VK_RMENU) & 0x8000 )!=0;
    snapshot.lShift=( GetAsyncKeyState(VK_LSHIFT) & 0x8000 )!=0;
    snapshot.rShift=( GetAsyncKeyState(VK_RSHIFT) & 0x8000 )!=0;
    snapshot.lMeta=( GetAsyncKeyState(VK_LWIN) & 0x8000 )!=0;
    snapshot.rMeta=( GetAsyncKeyState(VK_RWIN) & 0x8000 )!=0;

    if(snapshot.lCtrl) sendVirtualKey(VK_LCONTROL,true);
    if(snapshot.rCtrl) sendVirtualKey(VK_RCONTROL,true);
    if(snapshot.lAlt) sendVirtualKey(VK_LMENU,true);
    if(snapshot.rAlt) sendVirtualKey(VK_RMENU,true);
    if(snapshot.lShift) sendVirtualKey(VK_LSHIFT,true);
    if(snapshot.rShift) sendVirtualKey(VK_RSHIFT,true);
    if(snapshot.lMeta) sendVirtualKey(VK_LWIN,true);
    if(snapshot.rMeta) sendVirtualKey(VK_RWIN,true);
    return snapshot;
}

void restorePressedPhysicalModifiers(const QuickSayModifierSnapshot & snapshot){
    if(snapshot.lCtrl) sendVirtualKey(VK_LCONTROL);
    if(snapshot.rCtrl) sendVirtualKey(VK_RCONTROL);
    if(snapshot.lAlt) sendVirtualKey(VK_LMENU);
    if(snapshot.rAlt) sendVirtualKey(VK_RMENU);
    if(snapshot.lShift) sendVirtualKey(VK_LSHIFT);
    if(snapshot.rShift) sendVirtualKey(VK_RSHIFT);
    if(snapshot.lMeta) sendVirtualKey(VK_LWIN);
    if(snapshot.rMeta) sendVirtualKey(VK_RWIN);
}

void beginQuickSayPressBlock(){
    g_quickSayPressBlockCount++;
}

void endQuickSayPressBlock(){
    if(g_quickSayPressBlockCount>0) g_quickSayPressBlockCount--;
}

void appendTextOutputAction(QVector<QuickSayOutputAction> & actions,const QString & text){
    if(text.isEmpty()) return;
    QuickSayOutputAction action;
    action.type=QuickSayOutputActionType::Text;
    action.text=text;
    actions.append(action);
}

bool virtualKeyFromName(const QString & name,WORD & vk){
    QString key=name.trimmed().toLower();
    if(key=="enter" || key=="return"){
        vk=VK_RETURN;
        return true;
    }
    if(key=="tab"){
        vk=VK_TAB;
        return true;
    }
    if(key=="space"){
        vk=VK_SPACE;
        return true;
    }
    if(key=="esc" || key=="escape"){
        vk=VK_ESCAPE;
        return true;
    }
    if(key=="backspace"){
        vk=VK_BACK;
        return true;
    }
    if(key=="left"){
        vk=VK_LEFT;
        return true;
    }
    if(key=="right"){
        vk=VK_RIGHT;
        return true;
    }
    if(key=="up"){
        vk=VK_UP;
        return true;
    }
    if(key=="down"){
        vk=VK_DOWN;
        return true;
    }
    if(key=="ins" || key=="insert"){
        vk=VK_INSERT;
        return true;
    }
    if(key.size()==1){
        QChar c=key.at(0).toUpper();
        ushort u=c.unicode();
        if(u>='A' && u<='Z'){
            vk=static_cast<WORD>(u);
            return true;
        }
        if(u>='0' && u<='9'){
            vk=static_cast<WORD>(u);
            return true;
        }
    }
    if(key.size()>=2 && key.at(0)=='f'){
        bool ok=false;
        int n=key.mid(1).toInt(&ok);
        if(ok && n>=1 && n<=24){
            vk=static_cast<WORD>(VK_F1+n-1);
            return true;
        }
    }
    return false;
}

bool parsePressCombo(const QString & combo,QuickSayOutputAction & action){
    QStringList parts=combo.split('+',Qt::KeepEmptyParts);
    if(parts.isEmpty()) return false;

    bool hasCtrl=false;
    bool hasAlt=false;
    bool hasShift=false;
    bool hasMeta=false;
    bool hasPrimary=false;
    WORD primaryKey=0;

    for(const QString & rawPart:parts){
        QString part=rawPart.trimmed();
        if(part.isEmpty()) return false;
        QString lower=part.toLower();
        if(lower=="ctrl" || lower=="control"){
            if(hasCtrl) return false;
            hasCtrl=true;
            continue;
        }
        if(lower=="alt"){
            if(hasAlt) return false;
            hasAlt=true;
            continue;
        }
        if(lower=="shift"){
            if(hasShift) return false;
            hasShift=true;
            continue;
        }
        if(lower=="meta" || lower=="win" || lower=="windows"){
            if(hasMeta) return false;
            hasMeta=true;
            continue;
        }

        if(hasPrimary) return false;
        if(!virtualKeyFromName(part,primaryKey)) return false;
        hasPrimary=true;
    }

    if(!hasPrimary) return false;
    action.type=QuickSayOutputActionType::Press;
    if(hasCtrl) action.modifiers.append(VK_LCONTROL);
    if(hasAlt) action.modifiers.append(VK_LMENU);
    if(hasShift) action.modifiers.append(VK_LSHIFT);
    if(hasMeta) action.modifiers.append(VK_LWIN);
    action.key=primaryKey;
    return true;
}

bool parseSleepDurationMs(const QString & text,int & sleepMs){
    QString duration=text.trimmed();
    QString unit;
    if(duration.endsWith("ms",Qt::CaseInsensitive)){
        unit="ms";
        duration.chop(2);
    }
    else if(duration.endsWith("s",Qt::CaseInsensitive)){
        unit="s";
        duration.chop(1);
    }
    else{
        unit="s";
    }

    if(duration.isEmpty()) return false;
    bool ok=false;
    int value=duration.toInt(&ok);
    if(!ok || value<0) return false;

    if(unit=="s"){
        if(value>INT_MAX/1000) return false;
        value*=1000;
    }
    sleepMs=value;
    return true;
}

bool parseQuickSayTag(const QString & tag,QuickSayOutputAction & action){
    QString trimmed=tag.trimmed();
    if(trimmed.isEmpty()) return false;

    QString lower=trimmed.toLower();
    if(lower=="sleep"){
        action.type=QuickSayOutputActionType::Sleep;
        action.sleepMs=1000;
        return true;
    }

    QStringList parts=trimmed.split(' ',Qt::SkipEmptyParts);
    if(parts.size()==2 && parts.at(0).compare("sleep",Qt::CaseInsensitive)==0){
        int sleepMs=0;
        if(!parseSleepDurationMs(parts.at(1),sleepMs)) return false;
        action.type=QuickSayOutputActionType::Sleep;
        action.sleepMs=sleepMs;
        return true;
    }

    if(parts.size()==2 && parts.at(0).compare("press",Qt::CaseInsensitive)==0){
        return parsePressCombo(parts.at(1),action);
    }

    if(trimmed.size()>3 &&
       trimmed.left(3).compare("img",Qt::CaseInsensitive)==0 &&
       trimmed.at(3).isSpace()){
        QString path=trimmed.mid(4).trimmed();
        if(   (path.startsWith('"') && path.endsWith('"')) ||
              (path.startsWith('\'') && path.endsWith('\''))   ){
            path=path.mid(1,path.size()-2);
        }
        QFileInfo info(path);
        if(!info.isAbsolute() || !info.exists() || !info.isFile()) return false;
        QImage image(path);
        if(image.isNull()) return false;
        action.type=QuickSayOutputActionType::Image;
        action.image=image;
        action.imagePath=info.absoluteFilePath();
        return true;
    }

    if(lower=="enter" || lower=="tab" || lower=="space" || lower=="esc" ||
       lower=="backspace" || lower=="left" || lower=="right" ||
       lower=="up" || lower=="down"){
        return parsePressCombo(trimmed,action);
    }

    return false;
}

bool parseQuickSayOutputActions(const QString & text,QVector<QuickSayOutputAction> & actions){
    QString buffer;
    for(int i=0;i<text.size();){
        if(text.at(i)=='\\' && i+1<text.size() && text.at(i+1)=='<'){
            buffer.append('<');
            i+=2;
            continue;
        }

        if(text.at(i)=='<'){
            int closeIndex=text.indexOf('>',i+1);
            if(closeIndex<0){
                buffer.append('<');
                i++;
                continue;
            }
            QuickSayOutputAction action;
            if(!parseQuickSayTag(text.mid(i+1,closeIndex-i-1),action)){
                buffer.append(text.mid(i,closeIndex-i+1));
                i=closeIndex+1;
                continue;
            }
            appendTextOutputAction(actions,buffer);
            buffer.clear();
            actions.append(action);
            i=closeIndex+1;
            continue;
        }

        buffer.append(text.at(i));
        i++;
    }
    appendTextOutputAction(actions,buffer);
    return true;
}

void sendPressAction(const QuickSayOutputAction & action){
    for(WORD modifier:action.modifiers){
        sendVirtualKey(modifier);
    }
    sendVirtualKey(action.key);
    sendVirtualKey(action.key,true);
    for(int i=action.modifiers.size()-1;i>=0;i--){
        sendVirtualKey(action.modifiers.at(i),true);
    }
}

class QuickSayOutputRunner:public QObject{
private:
    QVector<QuickSayOutputAction> actions_;
    int index_=0;
public:
    QuickSayOutputRunner(const QVector<QuickSayOutputAction> & actions,QObject * parent=nullptr):QObject(parent),actions_(actions){}

    void start(){
        runCurrentAction();
    }
private:
    void finishOutput(){
        g_quickSayIsOutputting=false;
        deleteLater();
    }

    void finishAction(){
        if(index_>=actions_.size()){
            finishOutput();
            return;
        }
        QTimer::singleShot(   config["delay"].toInt()   ,[&](){ //动作之间等多久
            runCurrentAction();
        });
    }

    void runCurrentAction(){
        if(index_>=actions_.size()){
            finishOutput();
            return;
        }

        QuickSayOutputAction action=actions_.at(index_);
        index_++;
        switch(action.type){
        case QuickSayOutputActionType::Text:{
            QuickSayModifierSnapshot snapshot=releasePressedPhysicalModifiers();
            QApplication::clipboard()->setText(action.text);
            moniCtrlV();
            restorePressedPhysicalModifiers(snapshot);
            finishAction();
            break;
        }
        case QuickSayOutputActionType::Image:{
            QTimer::singleShot(1000,[&,action](){ //在微信输出图片时，有时会出现一个bug，经测试，在<img>标签前面加一个<sleep>能有效解决这个bug。于是我就实现了个：当用户输入<img>标签后，先延迟1秒，再来执行<img>标签的操作
                QuickSayModifierSnapshot snapshot=releasePressedPhysicalModifiers();
                setClipboardImageFileWin32(action);
                moniCtrlV();
                restorePressedPhysicalModifiers(snapshot);
                finishAction();
            });
            break;
        }
        case QuickSayOutputActionType::Press:{
            QuickSayModifierSnapshot snapshot=releasePressedPhysicalModifiers();
            beginQuickSayPressBlock();
            sendPressAction(action);
            restorePressedPhysicalModifiers(snapshot);
            QTimer::singleShot(   config["delay"].toInt()   ,[&](){ //程序在模拟按键后的动作延迟时间内，临时阻止 QuickSay 自己的键盘浏览逻辑处理这些按键，避免你模拟出来的按键又被 QuickSay 当成用户输入捕获。 //虽然这两个config["delay"].toInt()延迟语义不完全一样。但改成这样更方便管理
                endQuickSayPressBlock();
                finishAction();
            });
            break;
        }
        case QuickSayOutputActionType::Sleep:{
            QTimer::singleShot(action.sleepMs,[&](){
                finishAction();
            });
            break;
        }
        }
    }
};

void startQuickSayOutput(const QString & text){
    if(g_quickSayIsOutputting) return;

    QVector<QuickSayOutputAction> actions;
    if(!parseQuickSayOutputActions(text,actions)){
        QuickSayOutputAction action;
        action.type=QuickSayOutputActionType::Text;
        action.text=text;
        actions.append(action);
    }
    g_quickSayIsOutputting=true;
    QuickSayOutputRunner * runner=new QuickSayOutputRunner(actions,qApp);
    runner->start();
}

void shuchu(const QListWidgetItem * item,QWidget * chuangkou){
    QString text=item->data(Qt::UserRole).toString();//获取对应选项里的短语
    if(config["tudingflag"].toBool()==false) chuangkou->close();//如果没有钉住窗口，那么关闭窗口到托盘
    startQuickSayOutput(text);
}

void rebuildItemHotkeys(QListWidget & liebiao,QVector<QHotkey *> & itemHotkeys,QApplication * a){ //先禁用当前已注册的QHotkey *对象，然后遍历列表中的所有项，为它们注册快捷键
    for(auto hk:itemHotkeys){ //遍历动态数组中所有短语项对应的QHotkey *对象
        if(hk){ //如果该QHotkey *对象不是空指针
            hk->setRegistered(false);//禁用当前已注册的这个QHotkey *对象
            delete hk;//释放这个QHotkey *对象的内存
        }
    }
    itemHotkeys.clear();//清空这个动态数组

    for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
        QListWidgetItem * it=liebiao.item(i);//取出该短语项的指针，赋值给it
        QString hkStr=it->data(Qt::UserRole+3).toString();//返回保存在该短语项的Qt::UserRole+3里的快捷键字符串，用hkStr接收
        if(!hkStr.isEmpty()){ //如果hkStr不为空字符串
            QHotkey * hk=new QHotkey(QKeySequence(hkStr),true,a);//定义一个QHotkey *对象，设置快捷键为hkStr，全局可用。此时就成功注册快捷键了，也就是说按下快捷键会发出信号
            //设置按下快捷键后会怎样，即输出该短语项里的短语
            QObject::connect(hk,&QHotkey::activated,
                             [it](){
                                 QString text=it->data(Qt::UserRole).toString();//获取该短语项里的短语
                                 startQuickSayOutput(text);
                             }
                            );
            itemHotkeys.append(hk);//把QHotkey *对象hk添加到动态数组中
        }
        else{ //如果hkStr为空字符串
            itemHotkeys.append(nullptr);//那就把nullptr添加到动态数组中
        }
    }
}

void applyMainWindowNoActivate(){
    if(!pchuangkou) return;
    HWND hwnd=(HWND)pchuangkou->winId();
    LONG_PTR exStyle=GetWindowLongPtr(hwnd,GWL_EXSTYLE);
    SetWindowLongPtr(hwnd,GWL_EXSTYLE,exStyle | WS_EX_NOACTIVATE);
}

void setMainWindowNoActivateEnabled(bool enabled){
    if(!pchuangkou) return;
    HWND hwnd=(HWND)pchuangkou->winId();
    LONG_PTR exStyle=GetWindowLongPtr(hwnd,GWL_EXSTYLE);
    if(enabled){
        exStyle|=WS_EX_NOACTIVATE;
    }
    else{
        exStyle&=~WS_EX_NOACTIVATE;
    }
    SetWindowLongPtr(hwnd,GWL_EXSTYLE,exStyle);
    SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void stopQuickSayKeyboardHook(){
    if(g_keyboardHook){
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook=nullptr;
    }
}

void startQuickSayKeyboardHook();

void enterSearchMode(){
    if(!pchuangkou || !g_search) return;

    g_searchMode=true;
    g_lastForegroundBeforeSearch=GetForegroundWindow();
    stopQuickSayKeyboardHook();
    setMainWindowNoActivateEnabled(false);

    if(!pchuangkou->isVisible()) pchuangkou->show();
    pchuangkou->raise();
    pchuangkou->activateWindow();
    g_search->setFocus(Qt::ShortcutFocusReason);
    g_search->selectAll();
}

void leaveSearchMode(bool restorePreviousFocus=true){
    if(!pchuangkou || !g_search) return;

    g_searchMode=false;
    g_search->clearFocus();
    setMainWindowNoActivateEnabled(true);
    applyMainWindowNoActivate();
    startQuickSayKeyboardHook();

    if(restorePreviousFocus && g_lastForegroundBeforeSearch && IsWindow(g_lastForegroundBeforeSearch)){
        SetForegroundWindow(g_lastForegroundBeforeSearch);
    }
    g_lastForegroundBeforeSearch=nullptr;
}

QListWidgetItem * visibleItemByBadgeIndex(int targetIndex){
    if(!g_liebiao) return nullptr;
    int visibleCount=0;
    for(int i=0;i<g_liebiao->count();i++){
        QListWidgetItem * item=g_liebiao->item(i);
        if(!item->isHidden()){
            visibleCount++;
            if(visibleCount==targetIndex) return item;
        }
    }
    return nullptr;
}

void moveCurrentVisibleItem(int direction){
    if(!g_liebiao) return;
    int currentRow=g_liebiao->currentRow();
    int row=currentRow;
    while(true){
        row+=direction;
        if(row<0 || row>=g_liebiao->count()) return;
        QListWidgetItem * item=g_liebiao->item(row);
        if(item && !item->isHidden()){
            g_liebiao->setCurrentItem(item);
            g_liebiao->scrollToItem(item,QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

bool hasQuickSayBlockingWindow(){
    if(g_quickSayPressBlockCount>0) return true;
    if(QApplication::activeModalWidget() || QApplication::activePopupWidget()) return true;
    return (g_shezhichuangkou && g_shezhichuangkou->isVisible()) ||
           (g_tianjiachuangkou && g_tianjiachuangkou->isVisible()) ||
           (g_xiugaichuangkou && g_xiugaichuangkou->isVisible());
}

bool handleQuickSayBrowseKey(DWORD vkCode){
    if(g_searchMode) return false;
    if(!pchuangkou || !pchuangkou->isVisible()) return false;
    if(hasQuickSayBlockingWindow()) return false;

    bool hasSystemModifier=(GetAsyncKeyState(VK_CONTROL)&0x8000) ||
                           (GetAsyncKeyState(VK_MENU)&0x8000) ||
                           (GetAsyncKeyState(VK_LWIN)&0x8000) ||
                           (GetAsyncKeyState(VK_RWIN)&0x8000);

    switch(vkCode){
    case VK_TAB:
        enterSearchMode();
        return true;
    case VK_ESCAPE:
        pchuangkou->close();
        return true;
    case VK_RETURN:
        if(g_liebiao && g_liebiao->currentItem()){
            shuchu(g_liebiao->currentItem(),pchuangkou);
            return true;
        }
        return false;
    case VK_LEFT:
        if(g_tabBar) g_tabBar->setCurrentIndex(qMax(0,g_tabBar->currentIndex()-1));
        return true;
    case VK_RIGHT:
        if(g_tabBar) g_tabBar->setCurrentIndex(qMin(g_tabBar->count()-1,g_tabBar->currentIndex()+1));
        return true;
    case VK_UP:
        moveCurrentVisibleItem(-1);
        return true;
    case VK_DOWN:
        moveCurrentVisibleItem(1);
        return true;
    default:
        break;
    }

    int targetIndex=-1;
    if(vkCode>='1' && vkCode<='9') targetIndex=vkCode-'0';
    else if(vkCode=='0') targetIndex=10;
    else if(vkCode>='A' && vkCode<='Z') targetIndex=vkCode-'A'+11;

    if(targetIndex!=-1 && !hasSystemModifier){
        QListWidgetItem * item=visibleItemByBadgeIndex(targetIndex);
        if(item){
            shuchu(item,pchuangkou);
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK quickSayKeyboardProc(int code,WPARAM wParam,LPARAM lParam){
    if(code==HC_ACTION && (wParam==WM_KEYDOWN || wParam==WM_SYSKEYDOWN)){
        KBDLLHOOKSTRUCT * keyInfo=reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if(handleQuickSayBrowseKey(keyInfo->vkCode)) return 1;
    }
    return CallNextHookEx(g_keyboardHook,code,wParam,lParam);
}

void startQuickSayKeyboardHook(){
    if(!g_searchMode && !g_keyboardHook){
        g_keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,quickSayKeyboardProc,GetModuleHandleW(nullptr),0);
    }
}

void showMainWindowNoActivate(QWidget & chuang){
    g_searchMode=false;
    chuang.setAttribute(Qt::WA_ShowWithoutActivating,true);
    applyMainWindowNoActivate();
    if(!chuang.isVisible()) chuang.show();
    HWND hwnd=(HWND)chuang.winId();
    applyMainWindowNoActivate();
    ShowWindow(hwnd,SW_SHOWNOACTIVATE);
    HWND insertAfter=config["zhiding"].toBool()?HWND_TOPMOST:HWND_TOP;
    SetWindowPos(hwnd,insertAfter,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    startQuickSayKeyboardHook();
}

class NoActivateNativeFilter:public QAbstractNativeEventFilter{
public:
    bool nativeEventFilter(const QByteArray &,void * message,qintptr * result) override{
        if(!pchuangkou) return false;
        MSG * msg=reinterpret_cast<MSG *>(message);
        if(msg->message==WM_MOUSEACTIVATE){
            if(g_searchMode) return false;
            HWND mainHwnd=(HWND)pchuangkou->winId();
            if(g_search){
                HWND searchHwnd=(HWND)g_search->winId();
                if(msg->hwnd==searchHwnd || IsChild(searchHwnd,msg->hwnd)){
                    enterSearchMode();
                    *result=MA_ACTIVATE;
                    return true;
                }
            }
            if(msg->hwnd==mainHwnd || IsChild(mainHwnd,msg->hwnd)){
                *result=MA_NOACTIVATE;
                return true;
            }
        }
        return false;
    }
};

void xianshi(QWidget & chuang){ //如果窗口当前不可见，那么显示窗口，同时把窗口拉到屏幕最前方，并获得焦点
    if(pchuangkou && &chuang==pchuangkou){
        showMainWindowNoActivate(chuang);
        return;
    }
    if(!chuang.isVisible()) chuang.show();
    chuang.activateWindow();//把窗口拉到屏幕最前方，并获得焦点
}

bool isValidHotkey(const QKeySequence & seq,QVector<QHotkey *> & itemHotkeys_,QKeySequenceEdit * edit_,QHotkey * selfhk=nullptr){ //检查快捷键是否合规，如果不合规那么弹出对应警告对话框。规则：1.包含至少一个修饰键（Ctrl/Alt/Shift/Meta），同时有且只能有一个主键；或者可以是一个单独的F1~F11、Insert；2.快捷键不能已经存在动态数组itemHotkeys里，也就是说不能已被短语项使用；3.其他情况，比如用户输入了全局快捷键or已经被其他软件占用的快捷键，那么快捷键输入框会直接失去焦点，导致输入不了最后一个主键，也就是输入为空。这些情况就不用考虑了
    if(seq.isEmpty()){ //如果快捷键为空 //这个if就是以防万一用的，正常情况下不可能触发这个if
        QMessageBox::warning(edit_,"快捷键不合规","快捷键不能为空  ");//弹出警告对话框（多了两个空格是因为要留出空间，保持美观）
        return false;
    }
    if(seq.count()!=1){ //如果快捷键为多个快捷键的组合
        QMessageBox::warning(edit_,"快捷键不合规","快捷键不能为多个快捷键的组合  ");
        return false;
    }
    QString seqStr=seq.toString();
    bool hasModifier=false;
    if(   seqStr.contains("Ctrl")||seqStr.contains("Alt")||seqStr.contains("Shift")||seqStr.contains("Meta")   ) hasModifier=true;//判断是否包含至少一个修饰键（Ctrl/Alt/Shift/Meta）
    QString lastseqStr=seqStr.split('+').last();//seqStr.split('+')返回一个字符串数组，然后我们用last()取出它的最后一个元素
    bool hasPrimary=false;
    if(   lastseqStr!="Ctrl"&&lastseqStr!="Alt"&&lastseqStr!="Shift"&&lastseqStr!="Meta"   ) hasPrimary=true;//如果最后一个元素不是修饰键，那么判断为包含主键 //因为快捷键输入框的特殊性，所以就不用判断是否只有一个主键了。要我说其实判断是否包含主键这步都可以省略
    bool hasqita=false;
    if(   seqStr=="F1"||seqStr=="F2"||seqStr=="F3"||seqStr=="F4"||seqStr=="F5"||seqStr=="F6"||seqStr=="F7"||seqStr=="F8"||seqStr=="F9"||seqStr=="F10"||seqStr=="F11"||seqStr=="Ins"   ) hasqita=true;//判断是不是一个单独的F1~F11、Insert
    if( hasModifier&&hasPrimary || hasqita ){ //如果快捷键字符串满足要求
        for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象 //这里使用的是引用遍历
            if(hk){ //如果hk不为空指针
                if(hk==selfhk) continue;//如果hk等于传入的selfhk，那么说明用户没有修改快捷键，需要跳过，否则就会自己和自己冲突
                if(hk->shortcut()==seq){ //返回hk对应的QKeySequence对象，如果它和seq完全相同，那么说明快捷键已被使用
                    QMessageBox::warning(edit_,"快捷键不合规","快捷键已被使用  ");
                    return false;
                }
            }
        }
        return true;//如果遍历完成后都没有return，那么返回true
    }
    else{
        QMessageBox::warning(edit_,"快捷键不合规","快捷键必须包含至少一个修饰键（Ctrl/Alt/Shift/Meta）和一个主键  \n或者是一个单独的F1~F11、Insert  ");
        return false;
    }
}

class NoEscCloseWidget:public QWidget{
public:
    using QWidget::QWidget;
protected:
    void keyPressEvent(QKeyEvent * event) override{
        if(event->key()==Qt::Key_Escape){
            event->accept();
            return ;
        }
        QWidget::keyPressEvent(event);
    }
};

void showAdvancedInputHelp(QWidget & parent){
    NoEscCloseWidget * helpWindow=new NoEscCloseWidget();
    helpWindow->setAttribute(Qt::WA_DeleteOnClose);
    helpWindow->setWindowTitle("如何更高级地输入？");
    helpWindow->setWindowIcon(parent.windowIcon());
    helpWindow->setFixedSize(620,520);

    QPlainTextEdit * helpEdit=new QPlainTextEdit(helpWindow);
    helpEdit->setReadOnly(true);
    helpEdit->setPlainText(
R"(规则：
<img 图片绝对路径>
<press Enter>
<press Return> #等价于<press Enter>
<press Tab>
<press Space>
<press Esc>
<press Escape> #等价于<press Esc>
<press Backspace>
<press Left>
<press Right>
<press Up>
<press Down>
<press Insert>
<press Ins> #等价于<press Insert>
<press A> #支持A-Z
<press 0> #支持0-9
<press F1> #支持F1-F24
<press Meta+Ctrl+Alt+Shift+A> #修饰键的顺序无所谓，因此该写法等价于<press Ctrl+Shift+Alt+Meta+A>
<sleep 500ms> #停顿500ms
<sleep 2s>
<sleep 2> #停顿2s，等价于<sleep 2s>

常用单键允许简写。也就是：
<Enter>
<Tab>
<Space>
<Esc>
<Backspace>
<Left>
<Right>
<Up>
<Down>
<sleep> #停顿1秒

支持的修饰键：
Ctrl #也可写作Control
Alt
Shift
Meta #也可写作Win或Windows


同时语法上支持转义，比如用户真的想输入<Enter>，可以写：“\<Enter>”

如果语法不合规，那么原样输出

例如：
“请详细讲述一下<Enter>相关<sleep>知识。”
于是在Ctrl+V输入“请详细讲述一下”之后，会模拟输入一个回车，Ctrl+V输入“相关”之后，会停顿1秒，最后Ctrl+V输入“知识。”
全程执行完当前步骤后，停顿 config["delay"].toInt() ms，再安排下一步骤。


提示一下用户，因为QQ、微信等软件图片上传需要时间，所以如果要使用<img><Enter>标签，那么最好在后面加一个<sleep>)"
    );
    helpEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    helpEdit->setGeometry(0,0,620,520);

    helpWindow->move(parent.x()+30,parent.y()+30);
    helpWindow->show();
    helpWindow->raise();
    helpWindow->activateWindow();
}

void adjustAllWindows(int w,int h, //根据宽高，设置所有窗口大小和所有控件大小，以及所有控件位置
                      QWidget & chuangkou,QListWidget & liebiao,QTabBar & tabBar,QLineEdit & search,QPushButton & shezhi,QPushButton & tianjia,QPushButton & tuding, //主窗口
                      QWidget & shezhichuangkou, //设置窗口
                      QWidget & tianjiachuangkou,QPlainTextEdit & tianjiakuang,QPushButton & tianjia_gaojishuru,QLabel & tianjia_beizhuwenben,QPlainTextEdit & tianjia_beizhukuang,QLabel & tianjia_kjjwenben,QKeySequenceEdit & tianjia_kjjkuang,QPushButton & tianjia_kjjqingkong,QPushButton & tianjiaquxiao,QPushButton & tianjiaqueding, //添加窗口
                      QWidget & xiugaichuangkou,QPlainTextEdit & xiugaikuang,QPushButton & xiugai_gaojishuru,QLabel & xiugai_beizhuwenben,QPlainTextEdit & xiugai_beizhukuang,QLabel & xiugai_kjjwenben,QKeySequenceEdit & xiugai_kjjkuang,QPushButton & xiugai_kjjqingkong,QPushButton & xiugaiquxiao,QPushButton & xiugaiqueding //修改窗口
                     ){
    //主窗口
    chuangkou.setFixedSize(w,h);
    liebiao.move(0,46);
    liebiao.setFixedSize(w,h-40);//500*460 //把liebiao最下面的6个像素放到窗口外，隐藏起来。这样平行滚动条就不会不好看了
    tabBar.move(5,5);
    tabBar.setFixedSize(w-230,40);//270*40
    search.move(w-205,7);//295,7
    search.setFixedSize(90,35);
    shezhi.move(w-41,5);//459,5
    shezhi.setFixedSize(36,36);
    tianjia.move(w-77,5);//423,5
    tianjia.setFixedSize(36,36);
    tuding.move(w-113,5);//387,5
    tuding.setFixedSize(36,36);

    //设置窗口
    shezhichuangkou.setFixedSize(w,h);

    //添加窗口
    tianjiachuangkou.setFixedSize(w,h);
    tianjiakuang.move(0,0);
    tianjiakuang.setFixedSize(w,h*0.4);//500*200
    tianjia_gaojishuru.move(w*0.7,h*0.4);//350,200
    tianjia_gaojishuru.setFixedSize(147,35);
    tianjia_beizhuwenben.move(10,h*0.5-20);//10,230
    tianjia_beizhuwenben.setFixedSize(50,20);
    tianjia_beizhukuang.move(0,h*0.5);//0,250
    tianjia_beizhukuang.setFixedSize(w,h*0.25);//500*125
    tianjia_kjjwenben.move(10,h*0.8+7);//10,407
    tianjia_kjjwenben.setFixedSize(50,20);
    tianjia_kjjkuang.move(70,h*0.8);//70,400
    tianjia_kjjkuang.setFixedSize(w*0.4,38);//200*38
    tianjia_kjjqingkong.move(w*0.4+70,h*0.8);//270,400
    tianjia_kjjqingkong.setFixedSize(57,38);
    tianjiaquxiao.move(0,h-37);//0,463
    tianjiaquxiao.setFixedSize(w/2,33);//250*33
    tianjiaqueding.move(w/2,h-37);//250,463
    tianjiaqueding.setFixedSize(w/2,33);//250*33

    //修改窗口
    xiugaichuangkou.setFixedSize(w,h);
    xiugaikuang.move(0,0);
    xiugaikuang.setFixedSize(w,h*0.4);//500*200
    xiugai_gaojishuru.move(w*0.7,h*0.4);//350,200
    xiugai_gaojishuru.setFixedSize(147,35);
    xiugai_beizhuwenben.move(10,h*0.5-20);//10,230
    xiugai_beizhuwenben.setFixedSize(50,20);
    xiugai_beizhukuang.move(0,h*0.5);//0,250
    xiugai_beizhukuang.setFixedSize(w,h*0.25);//500*125
    xiugai_kjjwenben.move(10,h*0.8+7);//10,407
    xiugai_kjjwenben.setFixedSize(50,20);
    xiugai_kjjkuang.move(70,h*0.8);//70,400
    xiugai_kjjkuang.setFixedSize(w*0.4,38);//200*38
    xiugai_kjjqingkong.move(w*0.4+70,h*0.8);//270,400
    xiugai_kjjqingkong.setFixedSize(57,38);
    xiugaiquxiao.move(0,h-37);//0,463
    xiugaiquxiao.setFixedSize(w/2,33);//250*33
    xiugaiqueding.move(w/2,h-37);//250,463
    xiugaiqueding.setFixedSize(w/2,33);//250*33
}

//自定义一个事件过滤器类WindowMoveFilter，实现：当用户移动窗口时记录窗口位置。使得呼出窗口时让窗口在记录的位置显示
class WindowMoveFilter:public QObject{
private:
    QWidget * chuangkou;//指向主窗口
    QWidget * shezhichuangkou;//指向设置窗口
    QWidget * tianjiachuangkou;//指向添加窗口
    QWidget * xiugaichuangkou;//指向修改窗口
    QString configPath_;//指向config.json文件路径
public:
    WindowMoveFilter(QWidget * main,QWidget * shezhi,QWidget * tianjia,QWidget * xiugai,const QString & path,QObject * parent=nullptr):chuangkou(main),shezhichuangkou(shezhi),tianjiachuangkou(tianjia),xiugaichuangkou(xiugai),configPath_(path),QObject(parent){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{
        if(event->type()==QEvent::Move){ //如果是窗口移动事件
            if(obj==chuangkou){ //如果移动的是主窗口
                config["chuangkou_x"]=chuangkou->x();//记录主窗口在x轴上的位置
                config["chuangkou_y"]=chuangkou->y();//记录主窗口在y轴上的位置
                saveConfig(configPath_);//写入程序设置到config.json
            }
            else if(shezhichuangkou->isActiveWindow()){ //如果焦点在设置窗口
                config["shezhichuangkou_x"]=shezhichuangkou->x();//记录设置窗口在x轴上的位置
                config["shezhichuangkou_y"]=shezhichuangkou->y();//记录设置窗口在y轴上的位置
                saveConfig(configPath_);
            }
            else if(tianjiachuangkou->isActiveWindow()){ //如果焦点在添加窗口
                config["tianjiachuangkou_x"]=tianjiachuangkou->x();//记录添加窗口在x轴上的位置
                config["tianjiachuangkou_y"]=tianjiachuangkou->y();//记录添加窗口在y轴上的位置
                saveConfig(configPath_);
            }
            else if(xiugaichuangkou->isActiveWindow()){ //如果焦点在修改窗口
                config["xiugaichuangkou_x"]=xiugaichuangkou->x();//记录修改窗口在x轴上的位置
                config["xiugaichuangkou_y"]=xiugaichuangkou->y();//记录修改窗口在y轴上的位置
                saveConfig(configPath_);
            }
        }
        return QObject::eventFilter(obj,event);//其他事件走默认处理
    }
};

//自定义一个事件过滤器类MyEventFilter，实现：Esc键可以关闭主窗口/添加窗口/修改窗口；回车键Enter可以输出光标处短语；左右方向键可以切换标签
class MyEventFilter:public QObject{
private:
    QWidget * chuangkou;//指向主窗口
    QWidget * tianjiachuangkou;//指向添加窗口
    QWidget * xiugaichuangkou;//指向修改窗口
    QPushButton * tianjiaquxiao;//指向添加窗口的取消按钮
    QPushButton * xiugaiquxiao;//指向修改窗口的取消按钮
    QListWidget * liebiao;//指向主窗口里的列表
    QTabBar * tabBar;//指向主窗口里的标签栏
    QLineEdit * search;//指向主窗口里的搜索框
public:
    MyEventFilter(QWidget * main,QWidget * tianjia,QWidget * xiugai,QPushButton * tjqx,QPushButton * xgqx,QListWidget * l,QTabBar * t,QLineEdit * s):chuangkou(main),tianjiachuangkou(tianjia),xiugaichuangkou(xiugai),tianjiaquxiao(tjqx),xiugaiquxiao(xgqx),liebiao(l),tabBar(t),search(s){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{
        if(obj==search && event->type()==QEvent::FocusOut && g_searchMode){
            QTimer::singleShot(0,[](){
                if(g_searchMode && (!g_search || !g_search->hasFocus()) && (!pchuangkou || !pchuangkou->isActiveWindow())){
                    leaveSearchMode(false);
                }
            });
        }
        if(event->type()==QEvent::KeyPress){ //如果是键盘按下事件
            QKeyEvent * keyEvent=static_cast<QKeyEvent *>(event);
            if(g_searchMode && search->hasFocus()){
                if(keyEvent->key()==Qt::Key_Escape){
                    if(!search->text().isEmpty()) search->clear();
                    leaveSearchMode();
                    return true;
                }
                if(keyEvent->key()==Qt::Key_Tab){
                    leaveSearchMode();
                    return true;
                }
                if(keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter){
                    QListWidgetItem * item=liebiao->currentItem();
                    leaveSearchMode();
                    if(item){
                        shuchu(item,chuangkou);
                    }
                    return true;
                }
                if(keyEvent->key()==Qt::Key_Up){
                    moveCurrentVisibleItem(-1);
                    return true;
                }
                if(keyEvent->key()==Qt::Key_Down){
                    moveCurrentVisibleItem(1);
                    return true;
                }
                return false;
            }
            if( keyEvent->key()==Qt::Key_Tab && chuangkou->isActiveWindow() ){ //如果按下的是Tab键，并且焦点在主窗口
                if(search->hasFocus()) liebiao->setFocus();//如果焦点在搜索框，那么给列表焦点
                else search->setFocus();//如果焦点不在搜索框，那么给搜索框焦点
                return true;//拦截事件，不再向下传递
            }
            if(keyEvent->key()==Qt::Key_Escape){ //如果按下的是Esc键
                if(chuangkou->isActiveWindow()){ //如果焦点在主窗口
                    chuangkou->close();//关闭主窗口
                    return true;
                }
                else if(tianjiachuangkou->isActiveWindow()){ //如果焦点在添加窗口
                    tianjiaquxiao->click();//相当于按下“取消”按钮
                    return true;
                }
                else if(xiugaichuangkou->isActiveWindow()){ //如果焦点在修改窗口
                    xiugaiquxiao->click();//相当于按下“取消”按钮
                    return true;
                }
            }
            if(   ( keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter ) && chuangkou->isActiveWindow()   ){ //如果按下的是回车键，并且焦点在主窗口
                if(search->hasFocus()){ //如果焦点在搜索框
                    liebiao->setFocus();//给列表焦点
                    return true;
                }
                QListWidgetItem * item=liebiao->currentItem();//获取当前选中的短语项
                if(item){ //如果有选中的项，那么调用shuchu函数
                    shuchu(item,chuangkou);
                    return true;
                }
            }
            if(   keyEvent->key()==Qt::Key_Left && chuangkou->isActiveWindow()   ){ //如果按下的是左方向键，并且焦点在主窗口
                if(search->hasFocus()) liebiao->setFocus();//如果焦点在搜索框，那么给列表焦点
                tabBar->setCurrentIndex(   qMax(0,tabBar->currentIndex()-1)   );//设置选中标签为 当前选中标签左边的那个标签 
                return true;
            }
            if(   keyEvent->key()==Qt::Key_Right && chuangkou->isActiveWindow()   ){ //如果按下的是右方向键，并且焦点在主窗口
                if(search->hasFocus()) liebiao->setFocus();//如果焦点在搜索框，那么给列表焦点
                tabBar->setCurrentIndex(   qMin(tabBar->count()-1,tabBar->currentIndex()+1)   );//设置选中标签为 当前选中标签右边的那个标签 
                return true;
            }
            if(   keyEvent->key()==Qt::Key_Up && chuangkou->isActiveWindow()   ){ //如果按下的是上方向键，并且焦点在主窗口
                if(search->hasFocus()){ //如果焦点在搜索框
                    liebiao->setFocus();//给列表焦点
                    liebiao->setCurrentRow(   qMax(0,liebiao->currentRow()-1)   );//设置列表选中短语项为 当前选中短语项上边的那个短语项
                    return true;
                }
            }
            if(   keyEvent->key()==Qt::Key_Down && chuangkou->isActiveWindow()   ){ //如果按下的是下方向键，并且焦点在主窗口
                if(search->hasFocus()){ //如果焦点在搜索框
                    liebiao->setFocus();//给列表焦点
                    liebiao->setCurrentRow(   qMin(liebiao->count()-1,liebiao->currentRow()+1)   );//设置列表选中短语项为 当前选中短语项下边的那个短语项
                    return true;
                }
            }

            if( chuangkou->isActiveWindow() && !search->hasFocus() ){ //如果焦点在主窗口并且焦点不在搜索框
                int key=keyEvent->key();//获取按下的键值
                int targetIndex=-1;//计算按下的键值对应第几条短语
                if(key>=Qt::Key_1 && key<=Qt::Key_9){ //如果按下的是1~9
                    targetIndex=key-Qt::Key_0;//对应第1~9条短语
                }
                else if(key==Qt::Key_0){ //如果按下的是0
                    targetIndex=10;//对应第10条短语
                }
                else if(key>=Qt::Key_A && key<=Qt::Key_Z){ //如果按下的是A~Z
                    targetIndex=key-Qt::Key_A+11; //对应第11~36条短语
                }
                if(targetIndex!=-1){
                    int visibleCount=0;//用于计数当前遍历到的可见项
                    for(int i=0;i<liebiao->count();i++){
                        QListWidgetItem * item=liebiao->item(i);
                        if(!item->isHidden()){ //如果该短语项没有被隐藏
                            visibleCount++;
                            if(visibleCount==targetIndex){ //如果计数等于targetIndex
                                shuchu(item,chuangkou);
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return QObject::eventFilter(obj,event);//其他事件走默认处理
    }
};

//为短语列表单独处理悬停提示，避免无焦点主窗口下Qt默认的item tooltip不触发
class PhraseItemToolTipFilter:public QObject{
private:
    QListWidget * list_;
    QTimer showTimer_;
    QPoint lastViewportPos_;
    QPoint lastGlobalPos_;
    QListWidgetItem * lastItem_=nullptr;
public:
    PhraseItemToolTipFilter(QListWidget * list,QObject * parent=nullptr):QObject(parent),list_(list){
        showTimer_.setSingleShot(true);
        showTimer_.setInterval(250);//当鼠标悬停在短语项上时，延迟250毫秒后显示鼠标悬停提示
        QObject::connect(&showTimer_,&QTimer::timeout,[&](){
            showToolTipForLastItem();
        });
    }
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{
        if(!list_ || obj!=list_->viewport()) return QObject::eventFilter(obj,event);

        if(event->type()==QEvent::MouseMove){
            QMouseEvent * mouseEvent=static_cast<QMouseEvent *>(event);
            QListWidgetItem * item=list_->itemAt(mouseEvent->pos());
            if(item && !item->isHidden() && !item->toolTip().isEmpty()){
                if(item!=lastItem_ || mouseEvent->pos()!=lastViewportPos_){
                    QToolTip::hideText();
                    lastItem_=item;
                    lastViewportPos_=mouseEvent->pos();
                    lastGlobalPos_=list_->viewport()->mapToGlobal(mouseEvent->pos());
                    showTimer_.start();
                }
            }
            else{
                hideToolTip();
            }
            return false;
        }

        if(event->type()==QEvent::ToolTip){
            if(showToolTipForLastItem()) return true;
            hideToolTip();
            return true;
        }

        if(event->type()==QEvent::Leave ||
           event->type()==QEvent::Wheel ||
           event->type()==QEvent::MouseButtonPress ||
           event->type()==QEvent::MouseButtonRelease){
            hideToolTip();
        }

        return QObject::eventFilter(obj,event);
    }
private:
    bool showToolTipForLastItem(){
        if(!list_ || !lastItem_ || lastItem_->isHidden() || lastItem_->toolTip().isEmpty()) return false;
        QRect itemRect=list_->visualItemRect(lastItem_);
        if(!itemRect.contains(lastViewportPos_)) return false;
        QToolTip::showText(lastGlobalPos_,lastItem_->toolTip(),list_->viewport(),itemRect);
        return true;
    }

    void hideToolTip(){
        showTimer_.stop();
        lastItem_=nullptr;
        QToolTip::hideText();
    }
};

//为快捷键输入框tianjia_kjjkuang、xiugai_kjjkuang自定义一个事件过滤器类，用于拦截它们的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
class KjjHotkeyEditFilter:public QObject{
private:
    QKeySequenceEdit * edit_;//指向快捷键输入框xiugai_kjjkuang
    QVector<QHotkey *> & itemHotkeys_;//引用动态数组itemHotkeys
public:
    KjjHotkeyEditFilter(QKeySequenceEdit * e,QVector<QHotkey *> & itemHotkeys,QObject * parent=nullptr):edit_(e),itemHotkeys_(itemHotkeys),QObject(parent){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{ //重写eventFilter()以拦截事件
        if( obj==edit_ && event->type()==QEvent::FocusIn ){ //当xiugai_kjjkuang获得焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象 //这里使用的是引用遍历
                if(hk) hk->setRegistered(false);//如果hk不为空指针，那么禁用当前已注册的快捷键hk
            }
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点。此时用户可继续输入新快捷键
        }
        if( obj==edit_ && event->type()==QEvent::FocusOut ){ //当xiugai_kjjkuang失去焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象
                if(hk) hk->setRegistered(true);//如果hk不为空指针，那么恢复当前已注册的快捷键hk
            }
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点
        }
        return QObject::eventFilter(obj,event);//其他事件走默认处理
    }
};

//为快捷键输入框hotkeyEdit单独自定义一个事件过滤器类，用于拦截它的焦点事件，实现：1.当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复；2.当输入框获得焦点时立即禁用当前已注册的全局快捷键；3.当输入框失去焦点时判断用户输入的快捷键是否合规，合规的话就应用，不合规的话就恢复输入框为原始快捷键、弹出警告对话框
class HotkeyEditFilter:public QObject{
private:
    QKeySequenceEdit * edit_;//指向快捷键输入框hotkeyEdit
    QHotkey * hotkey_;//指向hotkey，就是那个QHotkey *对象
    QString configPath_;//指向config.json文件路径
    QVector<QHotkey *> & itemHotkeys_;//引用动态数组itemHotkeys
public:
    HotkeyEditFilter(QKeySequenceEdit * e,QHotkey * h,const QString & path,QVector<QHotkey *> & itemHotkeys,QObject * parent=nullptr):edit_(e),hotkey_(h),configPath_(path),itemHotkeys_(itemHotkeys),QObject(parent){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{ //重写eventFilter()以拦截事件
        if( obj==edit_ && event->type()==QEvent::FocusIn ){ //当hotkeyEdit获得焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象
                if(hk) hk->setRegistered(false);//如果hk不为空指针，那么禁用当前已注册的快捷键hk
            }
            if(hotkey_) hotkey_->setRegistered(false);//如果hotkey_不为空指针，那么禁用当前已注册的全局快捷键
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点。此时用户可继续输入新快捷键
        }
        if( obj==edit_ && event->type()==QEvent::FocusOut ){ //当hotkeyEdit失去焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象
                if(hk) hk->setRegistered(true);//如果hk不为空指针，那么恢复当前已注册的快捷键hk
            }
            QKeySequence seq=edit_->keySequence();//取出用户在输入框里输入的快捷键
            if(!seq.isEmpty()){ //如果输入不为空
                if(!isValidHotkey(seq,itemHotkeys_,edit_)){ //如果快捷键不合规（调用isValidHotkey函数检查快捷键是否合规）
                    edit_->setKeySequence( QKeySequence(config["hotkey"].toString()) );//恢复输入框为原始快捷键
                }
                else{ //如果快捷键合规
                    hotkey_->setShortcut(seq,true);//立即应用新快捷键
                    config["hotkey"]=seq.toString();//更新全局对象config
                    saveConfig(configPath_);//写入程序设置到config.json
                }
            }
            else{ //如果输入为空
                edit_->setKeySequence( QKeySequence(config["hotkey"].toString()) );//恢复输入框为原始快捷键
            }
            if(hotkey_) hotkey_->setRegistered(true);//如果hotkey_不为空指针，那么重新启用全局快捷键
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点
        }
        return QObject::eventFilter(obj,event);//其他事件走默认处理
    }
};

class BadgeDelegate:public QStyledItemDelegate{ //自定义一个委托类，用于在短语项左上角或者右上角绘制角标
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter * painter,const QStyleOptionViewItem & option, const QModelIndex & index) const override{ //重写绘制函数，用于自定义显示效果
        QStyledItemDelegate::paint(painter,option,index);//先调用默认的绘制方法，画好背景和文字
        QString badgeText=index.data(Qt::UserRole+4).toString();//通过数据模型，从当前短语项的Qt::UserRole+4中取出角标字符
        if(!badgeText.isEmpty()){ //如果角标不为空 //【【【注：想修改角标样式在这里修改】】】
            painter->save();//保存画笔状态
            painter->setRenderHint(QPainter::Antialiasing);//开启抗锯齿
            QFont font=painter->font();//获取当前画笔的字体
            font.setPixelSize(10);//设置字体大小为10像素
            font.setBold(true);//设置字体为粗体
            painter->setFont(font);//把设置好的字体应用给画笔
            painter->setBrush(QColor("#0078d4"));//设置角标背景色
            painter->setPen(Qt::NoPen);//绘制背景时无边框
            QRect badgeRect;//定义角标的矩形区域
            if(config["jiaobiao"].toBool()==true){ //设置角标位置和宽高
                badgeRect=QRect(option.rect.left()+4,option.rect.top()+4,18,18);//左上角，宽高为18像素
            }
            else{
                badgeRect=QRect(option.rect.right()-18-5,option.rect.top()+5,18,18);//右上角，宽高为18像素
            }
            painter->drawRoundedRect(badgeRect,5,5);//绘制圆角矩形背景
            painter->setPen(Qt::white);//设置字体颜色为白色
            painter->drawText(badgeRect,Qt::AlignCenter,badgeText);//绘制数字/字母，水平垂直居中显示
            painter->restore();//恢复画笔状态
        }
    }
};

//为tabBar单独自定义一个继承自QTabBar的子类，同时重写wheelEvent()实现滚动鼠标滚轮可以切换标签
class MyTabBar:public QTabBar{
public:
    using QTabBar::QTabBar;//直接继承QTabBar构造函数
protected:
    void wheelEvent(QWheelEvent * event) override{
        int delta=event->angleDelta().y();//获取鼠标滚轮的y方向角度增量。正值为向上滚，负值为向下滚
        if(delta>0){
            setCurrentIndex(   qMax(0,currentIndex()-1)   );//设置选中标签为 当前选中标签左边的那个标签 
        }
        else if(delta<0){
            setCurrentIndex(   qMin(count()-1,currentIndex()+1)   );//设置选中标签为 当前选中标签右边的那个标签 
        }
        event->accept();//标记事件已处理，防止继续传递
    }
};

//为shezhichuangkou单独自定义一个继承自QWidget的子类，同时重写showEvent()实现每次窗口show()的时候清除子控件的焦点，再把焦点交给窗口本身。这样窗口show()的时候就不会自动聚焦子控件了
class bujihuoChuangkou:public QWidget{
public:
    using QWidget::QWidget;//直接继承QWidget构造函数
protected:
    void showEvent(QShowEvent * e) override{
        QWidget::showEvent(e);//调用父类的showEvent，保持默认行为
        if(this->focusWidget()){ //如果在show()的时候，当前窗口内有子控件持有焦点 //this->focusWidget()表示获取当前窗口内的焦点控件
            this->focusWidget()->clearFocus();//清除该子控件的焦点
        }
        this->setFocus(Qt::OtherFocusReason);//把焦点设置到窗口本身，而不是子控件
    }
};

int main(int argc, char *argv[]){
    SingleApplication a(argc, argv);//将QApplication替换为SingleApplication。写了这句代码，就可以确保程序只有一个实例正在运行了。如果尝试启动第二个实例，那么会终止并通知第一个实例
    //当用户启动程序时，如果程序已经有实例正在运行，那么触发
    QObject::connect(&a,&SingleApplication::instanceStarted,
                     [](){
                         if(pchuangkou){
                             pchuangkou->move(config["chuangkou_x"].toInt(),config["chuangkou_y"].toInt());//把chuangkou移动到记录的位置
                             xianshi(*pchuangkou);
                         }
                     }
                    );
    //设置全局样式表，实现类似于Windows 11的现代UI风格
    a.setStyleSheet(R"(
    /*====================全局基础样式====================*/
    QWidget{
        background-color: #f3f3f3;                                                      /*主背景色：浅灰色*/
        font-family: "Segoe UI","Microsoft YaHei UI","PingFang SC",Arial,sans-serif;    /*字体优先级：Segoe UI（Windows系统默认UI字体）>微软雅黑UI版本>PingFang SC（苹果系统默认字体）>Arial>系统默认无衬线字体*/
        font-size: 9pt;                                                                 /*全局字体大小：9磅*/
        font-weight: 400;                                                               /*全局字体粗细*/
        color: #323130;                                                                 /*全局字体颜色：深灰色*/
        selection-background-color: #0078d4;                                            /*文本选中背景色：蓝色*/
        selection-color: #ffffff;                                                       /*文本选中字体色：白色*/
    }
    /*====================图标按钮样式（专门为设置、添加按钮写的样式）====================*/
    QPushButton#iconButton{
        background-color: transparent;              /*按钮背景：透明*/
        border: none;                               /*按钮边框：无边框，完全隐藏边框*/
        border-radius: 4px;                         /*按钮圆角*/
        padding: 0px;                               /*按钮内边距*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    QPushButton#iconButton:hover{                   /*鼠标悬停时的按钮样式*/
        background-color: #f9f9f9;                  /*悬停背景：白色*/
        border: none;                               /*无边框*/
    }
    QPushButton#iconButton:pressed{                 /*按钮被按下时的样式*/
        background-color: #f7f7f7;                  /*按下背景：深一点的白色*/
        border: none;                               /*无边框*/
    }
    QPushButton#iconButton:focus{                   /*按钮获得焦点时的样式*/
        border: none;                               /*无边框*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================按钮样式====================*/
    QPushButton{
        background-color: #ffffff;                  /*按钮背景：纯白色*/
        border: 1px solid #d1d1d1;                  /*按钮边框：1像素浅灰色*/
        border-radius: 4px;                         /*按钮圆角*/
        padding: 6px 12px;                          /*按钮内边距：上下6像素，左右12像素*/
        color: #323130;                             /*按钮字体颜色：深灰色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    QPushButton:hover{
        background-color: #f9f9f9;                  /*鼠标悬停背景：白色*/
        border-color: #c8c8c8;                      /*悬停边框：浅灰色*/
    }
    QPushButton:pressed{
        background-color: #e5f3ff;                  /*鼠标按下背景：白色*/
        border-color: #0078d4;                      /*按下边框：蓝色*/
        color: #005a9e;                             /*按下文字：深蓝色*/
    }
    QPushButton:focus{
        border-color: #0078d4;                      /*焦点边框：蓝色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================列表控件样式====================*/
    QListWidget{
        background-color: #fcfcfc;                  /*列表背景：微灰白色，与短语项的#ffffff形成微妙对比，让界面更有深度感和专业感*/
        border: 1px solid #e5e5e5;                  /*列表边框：1像素浅灰色，营造阴影效果*/
        border-radius: 6px;                         /*列表圆角*/
        padding: 4px;                               /*列表内边距：上下左右4像素*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
        selection-background-color: transparent;    /*禁用默认选中样式*/
        alternate-background-color: transparent;    /*禁用交替行背景色*/
    }
    QListWidget::item{                              /*因为短语字体颜色和备注字体颜色不同，所以不能在这里设置字体颜色，不然在这里设置的字体颜色会覆盖在其他地方设置的字体颜色*/
        background-color: #ffffff;                  /*短语项背景：纯白色*/
        border: 1px solid #e5e5e5;                  /*短语项边框：1像素浅灰色*/
        border-radius: 4px;                         /*短语项圆角，与列表圆角一致*/
        padding: 12px 16px;                         /*短语项内边距：上下12像素，左右16像素*//*【【【注：想修改短语项高度在这里也可以修改】】】*/
        margin: 2px 2px;                            /*短语项外边距：上下2像素，左右2像素*//*【【【注：想让短语项更紧凑一点在这里修改】】】*/
    }
    QListWidget::item:hover{
        background-color: #f9f9f9;                  /*鼠标悬停短语项背景：白色*/
        border-color: #d1d1d1;                      /*鼠标悬停短语项边框：浅灰色*/
    }
    QListWidget::item:selected{
        background-color: #e5f3ff;                  /*鼠标选中短语项背景：偏蓝一点的白色*/
        border-color: #0078d4;                      /*鼠标选中短语项边框：蓝色*/
        color: #005a9e;                             /*鼠标选中短语项字体颜色：深蓝色*/
    }
    QListWidget::item:selected:hover{
        background-color: #cce7ff;                  /*鼠标选中且悬停，短语项背景：浅蓝色*/
        border-color: #106ebe;                      /*鼠标选中且悬停，短语项边框：更深的蓝色*/
    }
    /*====================标签栏样式====================*/
    QTabBar{
        background-color: transparent;              /*标签栏背景：透明*/
        border: none;                               /*无边框*/
    }
    QTabBar::tab{
        background-color: #ffffff;                  /*标签背景：纯白色*/
        color: #323130;                             /*标签字体颜色：深灰色*/
        border: 1px solid #e5e5e5;                  /*标签边框：1像素浅灰色*/
        border-radius: 4px;                         /*标签圆角*/
        padding: 8px 16px;                          /*标签内边距：上下8像素，左右16像素*/
        margin: 2px 2px;                            /*标签外边距：上下2像素，左右2像素*/
        min-width: 30px;                            /*标签最小宽度*//*【【【注：想修改标签最小宽度在这里修改】】】*/
    }
    QTabBar::tab:hover{
        background-color: #f9f9f9;                  /*鼠标悬停标签背景：白色*/
        border-color: #d1d1d1;                      /*鼠标悬停标签边框：浅灰色*/
    }
    QTabBar::tab:selected{
        background-color: #e5f3ff;                  /*鼠标选中标签背景：偏蓝一点的白色*/
        border-color: #0078d4;                      /*鼠标选中标签边框：蓝色*/
        color: #005a9e;                             /*鼠标选中标签字体颜色：深蓝色*/
    }
    QTabBar::tab:selected:hover{
        background-color: #cce7ff;                  /*鼠标选中且悬停，标签背景：浅蓝色*/
        border-color: #106ebe;                      /*鼠标选中且悬停，标签边框：更深的蓝色*/
    }
    /*====================输入框样式====================*/
    QLineEdit,QPlainTextEdit,QTextEdit{
        background-color: #ffffff;                  /*输入框背景：纯白色*/
        border: 2px solid #e5e5e5;                  /*输入框边框：2像素浅灰色*/
        border-radius: 4px;                         /*输入框圆角：4像素*/
        padding: 8px 12px;                          /*输入框内边距：上下8像素，左右12像素*/
        color: #323130;                             /*输入框字体颜色：深灰色*/
        selection-background-color: #0078d4;        /*选中背景：蓝色*/
        selection-color: #ffffff;                   /*选中文字：白色*/
    }
    QLineEdit:focus,QPlainTextEdit:focus,QTextEdit:focus{
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================数字输入框样式====================*/
    QSpinBox{
        background-color: #ffffff;                  /*数字框背景：纯白色*/
        border: 2px solid #e5e5e5;                  /*数字框边框：2像素浅灰色*/
        border-radius: 4px;                         /*数字框圆角：4像素*/
        padding: 6px 8px;                           /*数字框内边距：上下6像素，左右8像素*//*【【【注：想修改数字输入框大小在这里修改】】】*/
        color: #323130;                             /*数字框字体颜色*/
    }
    QSpinBox:focus{
        border-color: #0078d4;                      /*焦点边框：蓝色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    QSpinBox::up-button,QSpinBox::down-button{      /*完全隐藏数字输入框的上下箭头*/
        width: 0px;
        height: 0px;
        border: none;
        background: none;
    }
    /*====================快捷键输入框样式====================*/
    QKeySequenceEdit QLineEdit{                     /*对快捷键输入框QKeySequenceEdit内部的QLineEdit设置样式*/
        background-color: #ffffff;                  /*快捷键框背景：纯白色*/
        border: 2px solid #e5e5e5;                  /*快捷键框边框：2像素浅灰色*/
        border-radius: 4px;                         /*快捷键框圆角：4像素*/
        padding: 8px 12px;                          /*快捷键框内边距：上下8像素，左右12像素*//*【【【注：想修改快捷键输入框大小在这里修改】】】*/
        color: #323130;                             /*快捷键框字体颜色*/
    }
    QKeySequenceEdit QLineEdit:focus{
        border-color: #0078d4;                      /*焦点边框：蓝色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================右键菜单样式====================*/
    QMenu {
        background-color: #fefefe;                  /*菜单背景：白色*/
        border: 1px solid #e5e5e5;                  /*菜单边框：1像素浅灰色*/
        padding: 3px 3px;                           /*菜单内边距：上下4像素，左右0像素*/
    }
    QMenu::item {
        padding: 8px 16px;                          /*菜单项内边距：上下8像素，左右16像素*/
        color: #222222;                             /*字体颜色*/
        min-width: 60px;                            /*菜单项最小宽度*/
    }
    QMenu::item:selected {
        background-color: #e5f3ff;                  /*菜单项选中背景：浅蓝色*/
    }
    QMenu::item:pressed {
        background-color: #cce7ff;                  /*菜单项按下背景：深蓝色*/
    }
    /*====================标签样式====================*/
    QLabel{
        color: #605e5c;                             /*标签字体颜色：深灰色*/
        padding: 2px 0px;                           /*标签内边距：上下2像素，左右0像素*/
    }
    /*====================滚动条样式====================*/
    QScrollBar:vertical{                            /*垂直滚动条*/
        background-color: #f3f3f3;                  /*背景颜色：浅灰色*/
        width: 8px;                                 /*滚动条宽度：8像素*/
        border-radius: 4px;                         /*滚动条圆角：4像素*/
        margin: 0px;                                /*滚动条外边距：0像素，也就是无*/
        border: none;                               /*去掉边框*/
    }
    QScrollBar::handle:vertical{
        background-color: #c7c7c7;                  /*滑块颜色：浅灰色*/
        border-radius: 4px;                         /*滑块圆角：4像素*/
        min-height: 20px;                           /*滑块最小高度：20像素，确保可拖拽*/
        margin: 2px 1px;                            /*滑块外边距：上下2像素，左右1像素*/
    }
    QScrollBar::handle:vertical:hover{
        background-color: #a6a6a6;                  /*鼠标悬停滑块颜色：灰色*/
    }
    QScrollBar::handle:vertical:pressed{
        background-color: #8a8a8a;                  /*鼠标按下滑块颜色：更深的灰色*/
    }
    QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{
        height: 0px;                                /*隐藏滚动条自带的那个箭头*/
    }
    QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{
        background-color: transparent;              /*滚动条轨道颜色：透明*/
    }
    QScrollBar:horizontal{                          /*平行滚动条*/
        background-color: #f3f3f3;                  /*背景颜色：浅灰色*/
        height: 8px;                                /*滚动条高度：8像素*/
        border-radius: 4px;                         /*滚动条圆角：4像素*/
        margin: 0px;                                /*滚动条外边距：0像素，也就是无*/
        border: none;                               /*去掉边框*/
    }
    QScrollBar::handle:horizontal{
        background-color: #c7c7c7;                  /*滑块颜色：浅灰色*/
        border-radius: 4px;                         /*滑块圆角：4像素*/
        min-width: 20px;                            /*滑块最小宽度：20像素，确保可拖拽*/
        margin: 1px 2px;                            /*滑块边距：上下1像素，左右2像素*/
    }
    QScrollBar::handle:horizontal:hover{
        background-color: #a6a6a6;                  /*鼠标悬停滑块颜色：灰色*/
    }
    QScrollBar::handle:horizontal:pressed{
        background-color: #8a8a8a;                  /*鼠标按下滑块颜色：更深的灰色*/
    }
    QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{
        width: 0px;                                 /*隐藏滚动条自带的那个箭头*/
    }
    QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{
        background-color: transparent;              /*滚动条轨道颜色：透明*/
    }
    /*====================悬停提示样式====================*/
    QToolTip{
        background-color: #ffffff;                  /*提示背景颜色：纯白色*/
        color: #222222;                             /*提示字体颜色：深灰色*/
        padding: 2px 1px;                           /*提示内边距：上下2像素，左右1像素*/
    }
    )");



    a.setQuitOnLastWindowClosed(false);//这里填false的话就是关闭窗口后让程序隐藏到托盘，继续在后台运行。此时如果不在托盘的右键菜单添加一个退出键，你就只能在任务管理器里关闭该程序了
    QString configPath=QCoreApplication::applicationDirPath()+"/config.json";//定义config.json文件路径 //QCoreApplication::applicationDirPath()返回的是可执行文件的目录路径（不包含文件名本身）
    loadConfig(configPath);//程序启动时调用loadConfig函数
    saveConfig(configPath);//然后调用saveConfig函数，兼容旧版本

    QWidget chuangkou;
    pchuangkou=&chuangkou;//创建主窗口时把地址赋值给全局指针，用于当用户启动程序时，如果已经有实例正在运行，那么显示正在运行的那个实例的主窗口
    a.installNativeEventFilter(new NoActivateNativeFilter());
    chuangkou.setWindowTitle("QuickSay");
    chuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));

    //短语列表
    QListWidget liebiao(&chuangkou);
    g_liebiao=&liebiao;
    QString dataPath=QCoreApplication::applicationDirPath()+"/data.json";//定义data.json文件路径
    loadListFromJson(liebiao,dataPath);//程序启动时调用loadListFromJson函数
    saveListToJson(liebiao,dataPath);//然后调用saveListToJson函数，兼容旧版本
    liebiao.setWordWrap(true);//开启列表的自动换行功能，这样当文本超出宽度时会自动折行
    liebiao.setUniformItemSizes(true);//告诉视图所有短语项的高度都相同，这样才能正常自动折行。同时这会跳过逐个计算短语项的高度的过程，提高程序性能
    liebiao.setItemDelegate(new BadgeDelegate(&liebiao));//给列表设置BadgeDelegate委托类对象，实现绘制角标
    liebiao.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);//开启像素级滚动
    liebiao.verticalScrollBar()->setSingleStep(config["gundong"].toInt());//设置滚动条滚动速度
    liebiao.setMouseTracking(true);
    liebiao.viewport()->setMouseTracking(true);
    liebiao.viewport()->installEventFilter(new PhraseItemToolTipFilter(&liebiao,&liebiao));
    QVector<QHotkey *> itemHotkeys;//创建一个动态数组，保存所有短语项对应的QHotkey *对象，并且保证它们的下标（0~n-1）与短语项的行号一一对应。因此如果对应短语项的快捷键字符串为空字符串，那么对应QHotkey *对象为空指针
    rebuildItemHotkeys(liebiao,itemHotkeys,&a);//程序启动时为liebiao中的短语项注册快捷键
    //实现liebiao选项拖动排序
    liebiao.setDragEnabled(true);//允许短语项被拖动
    liebiao.setAcceptDrops(true);//允许列表接收拖动放下的项
    liebiao.setDropIndicatorShown(true);//显示拖动放下时的指示器
    liebiao.setDefaultDropAction(Qt::MoveAction);//设置默认拖放行为为移动，而不是复制
    liebiao.setDragDropMode(QAbstractItemView::InternalMove);//设置内部移动模式，用户只能在列表内部拖动
    //当按下liebiao中的某个选项时，就调用shuchu函数
    QObject::connect(&liebiao,&QListWidget::itemClicked,
                     [&](QListWidgetItem * item){
                         if(g_searchMode){
                             leaveSearchMode();
                         }
                         shuchu(item,&chuangkou);
                     }
                    );
    //标签栏
    MyTabBar tabBar(&chuangkou);
    g_tabBar=&tabBar;
    QString tabPath=QCoreApplication::applicationDirPath()+"/tab.json";//定义tab.json文件路径
    loadTabFromJson(tabBar,tabPath);//程序启动时调用loadTabFromJson函数
    saveTabToJson(tabBar,tabPath);//然后调用saveTabToJson函数，兼容旧版本
    tabBar.setExpanding(false);//始终根据标签内容长度来确定标签宽度，而不是在短语较少时根据标签栏宽度强制拉伸标签宽度
    tabBar.setUsesScrollButtons(false);//不显示左右按钮
    tabBar.setDrawBase(false);//不绘制基座
    tabBar.setMovable(true);//允许通过拖动改变标签顺序
    //搜索框【【【【【这个搜索框的逻辑我还没重构，之后记得来重构
    QLineEdit search(&chuangkou);
    g_search=&search;
    search.setClearButtonEnabled(true);//开启一键清除按钮（输入框右边的小叉叉）
    //搜索框文字发生变化触发
    QObject::connect(&search,&QLineEdit::textChanged,
                     [&](const QString & text){
                        filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),text);//根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                        for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
                             if(!liebiao.item(i)->isHidden()){ //如果该短语项没有隐藏
                                 liebiao.setCurrentItem(liebiao.item(i));//设置该短语项为当前选中的短语项
                                 break;
                             }
                        }
                     }
                    );
    //用户拖动短语项完成后触发
    QObject::connect(liebiao.model(),&QAbstractItemModel::rowsMoved,
                     [&](){
                         saveListToJson(liebiao,dataPath);
                         rebuildItemHotkeys(liebiao,itemHotkeys,&a);//拖动完成后为liebiao中的短语项注册快捷键
                         filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),search.text());//拖动完成后根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                     }
                    );
    //用户拖动标签完成后触发
    QObject::connect(&tabBar,&QTabBar::tabMoved,
                     [&](){
                         saveTabToJson(tabBar,tabPath);
                     }
                    );

    liebiao.setStyleSheet(QString("QListWidget::item{ height: %1px; }").arg(   tabBar.tabData(tabBar.currentIndex()).toInt()   ));//程序启动时根据当前选中标签取出并应用短语项高度（注意程序启动时默认选中的是第一个标签。这是因为：在空标签栏里添加第一个标签（loadTabFromJson是一个个添加标签的），Qt就会自动选中该标签）
    filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),search.text());//程序启动时根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
    for(int i=0;i<liebiao.count();i++){ //然后选中没有隐藏的第一个短语项
        if(!liebiao.item(i)->isHidden()){ //如果该短语项没有隐藏
            liebiao.setCurrentItem(liebiao.item(i));//设置该短语项为当前选中的短语项
            break;
        }
    }
    //如果当前选中标签发生改变，那么根据当前选中标签过滤短语项，然后选中没有隐藏的第一个短语项
    QObject::connect(&tabBar,&QTabBar::currentChanged,
                     [&](int index){ //index是新选中标签的索引
                         if( index>=0 && index<tabBar.count() ){ //如果索引有效
                             liebiao.setStyleSheet(QString("QListWidget::item{ height: %1px; }").arg(   tabBar.tabData(index).toInt()   ));//根据当前选中标签取出并应用短语项高度
                             filterListByTab(liebiao,tabBar.tabText(index),search.text());//根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                             for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
                                 if(!liebiao.item(i)->isHidden()){ //如果该短语项没有隐藏
                                     liebiao.setCurrentItem(liebiao.item(i));//设置该短语项为当前选中的短语项
                                     break;
                                 }
                             }
                         }
                     }
                    );
    tabBar.setContextMenuPolicy(Qt::CustomContextMenu);//为tabBar设置自定义右键菜单
    //当右键tabBar时，执行lambda表达式
    QObject::connect(&tabBar,&QWidget::customContextMenuRequested,
                     [&](const QPoint & pos){
                         int index=tabBar.tabAt(pos);//根据点击位置，返回该位置处标签的索引（如果点到空白区域则返回-1）
                         if(index<0){ //如果点到空白区域，那么弹出菜单，上面有添加标签一个选项 //虽然它返回的是-1，但因为C++标准库很多函数“未找到”时返回的都是负数，而不仅仅是-1，所以这里还是填<0更让我放心一点
                             QMenu menu2;
                             QAction tianjia("添加标签",&menu2);
                             menu2.addAction(&tianjia);
                             QAction * selectedAction=menu2.exec(tabBar.mapToGlobal(pos));//在鼠标点击的位置弹出菜单，等待用户选择一个QAction
                             if(selectedAction==&tianjia){ //如果用户选了“添加标签”
                                 QString tabName="";
                                 int itemHeight=config["default_item_height"].toInt();//取出config里的默认短语项高度，用作弹窗输入框里的默认值
                                 bool ok=showTabDialog(chuangkou,"添加标签",tabName,itemHeight);//调用我们自定义的showTabDialog函数，于是tabName、itemHeight里面存储的就是用户输入的数据
                                 if( ok && !tabName.isEmpty() ){ //如果用户选了“确认”并且输入的标签名称不为空
                                     if(!isTabNameDuplicate(tabBar,tabName)){ //如果用户输入的标签名称不重复
                                         tabBar.addTab(tabName);//在标签栏末尾添加新标签
                                         tabBar.setTabData(tabBar.count()-1,itemHeight);//把用户输入的短语项高度存到该标签的tabData中
                                         tabBar.setCurrentIndex(tabBar.count()-1);//设置选中标签为该标签
                                         saveTabToJson(tabBar,tabPath);
                                     }
                                     else{ //如果用户输入的标签名称重复
                                         QMessageBox::warning(&chuangkou,"标签名重复","该标签名已存在，请使用其他名称  ");
                                         return ;//结束当前这个槽函数的执行
                                     }
                                 }
                             }
                         }
                         else{ //如果点到某个标签，那么弹出菜单，上面有在当前标签后添加标签、修改标签、删除标签三个选项
                             QMenu menu2;
                             QAction tianjia("在当前标签后添加标签",&menu2);
                             menu2.addAction(&tianjia);
                             QAction xiugai("修改标签",&menu2);
                             menu2.addAction(&xiugai);
                             QAction shanchu("删除标签",&menu2);
                             menu2.addAction(&shanchu);
                             QAction * selectedAction=menu2.exec(tabBar.mapToGlobal(pos));//在鼠标点击的位置弹出菜单，等待用户选择一个QAction
                             if(selectedAction==&tianjia){ //如果用户选了“在当前标签后添加标签”
                                 QString tabName="";
                                 int itemHeight=config["default_item_height"].toInt();//取出config里的默认短语项高度，用作弹窗输入框里的默认值
                                 bool ok=showTabDialog(chuangkou,"添加标签",tabName,itemHeight);//调用我们自定义的showTabDialog函数，于是tabName、itemHeight里面存储的就是用户输入的数据
                                 if( ok && !tabName.isEmpty() ){ //如果用户选了“确认”并且输入的标签名称不为空
                                     if(!isTabNameDuplicate(tabBar,tabName)){ //如果用户输入的标签名称不重复
                                         tabBar.insertTab(index+1,tabName);//在当前标签后添加新标签
                                         tabBar.setTabData(index+1,itemHeight);//把用户输入的短语项高度存到该标签的tabData中
                                         tabBar.setCurrentIndex(index+1);//设置选中标签为该标签
                                         saveTabToJson(tabBar,tabPath);
                                     }
                                     else{ //如果用户输入的标签名称重复
                                         QMessageBox::warning(&chuangkou,"标签名重复","该标签名已存在，请使用其他名称  ");
                                         return ;//结束当前这个槽函数的执行
                                     }
                                 }
                             }
                             else if(selectedAction==&xiugai){ //如果用户选了“修改标签”
                                 QString oldName=tabBar.tabText(index);//记录修改前的标签名称
                                 QString newName=oldName;//这个newName将会用作弹窗输入框里的默认文本
                                 int itemHeight=tabBar.tabData(index).toInt();//取出当前标签的短语项高度，用作弹窗输入框里的默认值
                                 bool ok=showTabDialog(chuangkou,"修改标签",newName,itemHeight);//调用我们自定义的showTabDialog函数，于是newName、itemHeight里面存储的就是用户输入的数据
                                 if( ok && !newName.isEmpty() ){ //如果用户选了“确认”并且输入的标签名称不为空
                                     if( !isTabNameDuplicate(tabBar,newName) || newName==oldName ){ //如果用户输入的标签名称不重复，或者和oldName一样
                                         tabBar.setTabText(index,newName);//设置索引为index处的标签名称
                                         tabBar.setTabData(index,itemHeight);//把用户输入的短语项高度存到该标签的tabData中
                                         if(index==tabBar.currentIndex()){ //如果修改的标签是当前正在显示的标签
                                             liebiao.setStyleSheet(QString("QListWidget::item{ height: %1px; }").arg(itemHeight));//那么立刻应用短语项高度
                                         }
                                         for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
                                             if(   liebiao.item(i)->data(Qt::UserRole+2).toString()   ==oldName){ //如果该短语项的标签名称等于修改前的标签名称，那么把该短语项的标签名称改为修改后的标签名称
                                                 liebiao.item(i)->setData(Qt::UserRole+2,newName);//把新标签名称存到当前项的Qt::UserRole+2
                                             }
                                         }
                                         saveTabToJson(tabBar,tabPath);
                                         saveListToJson(liebiao,dataPath);
                                         filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),search.text());//修改后根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                                     }
                                     else{ //如果用户给出的标签名称重复
                                         QMessageBox::warning(&chuangkou,"标签名重复","该标签名已存在，请使用其他名称  ");
                                         return ;//结束当前这个槽函数的执行
                                     }
                                 }
                             }
                             else if(selectedAction==&shanchu){ //如果用户选了“删除标签”
                                 if(tabBar.count()<=1){
                                     QMessageBox::warning(&chuangkou,"提示","至少需要保留一个标签  ");
                                     return ;
                                 }
                                 QString deletedTabName=tabBar.tabText(index);//记录待删除的标签名称
                                 int ret=QMessageBox::question(&chuangkou,"确认删除","确定要删除标签吗？该标签下所有短语也会被删除  ");//弹出询问弹窗，并获取用户点击的选项
                                 if(ret==QMessageBox::Yes){
                                     tabBar.removeTab(index);//删除索引为index处的标签
                                     for(int i=liebiao.count()-1;i>=0;i--){ //倒序遍历列表中的所有项，避免删除时索引错乱
                                         if(   liebiao.item(i)->data(Qt::UserRole+2).toString()   ==deletedTabName){ //如果该短语项的标签名称等于待删除的标签名称，那么把该短语项删除
                                             delete liebiao.item(i);
                                         }
                                     }
                                     saveTabToJson(tabBar,tabPath);
                                     saveListToJson(liebiao,dataPath);
                                     rebuildItemHotkeys(liebiao,itemHotkeys,&a);//删除后为liebiao中的短语项注册快捷键
                                     filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),search.text());//删除后根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                                 }
                             }
                         }
                     }
                    );

    //创建shezhichuangkou窗口
    bujihuoChuangkou shezhichuangkou;//使用我们自定义的那个子类bujihuoChuangkou创建
    g_shezhichuangkou=&shezhichuangkou;
    shezhichuangkou.setWindowTitle("QuickSay-设置");
    shezhichuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QFormLayout * formLayout=new QFormLayout(&shezhichuangkou);//创建一个表单布局，并直接作用于设置窗口

    //全局快捷键设置
    QKeySequenceEdit hotkeyEdit( QKeySequence(config["hotkey"].toString()) ,&shezhichuangkou);//创建一个快捷键输入框，用于输入快捷键。里面一开始就存放着config里的快捷键
    formLayout->addRow("全局快捷键：",&hotkeyEdit);//在表单布局中添加一行，左边是标签“全局快捷键：”，右边是快捷键输入框hotkeyEdit
    QHotkey * hotkey=new QHotkey( QKeySequence(config["hotkey"].toString()) ,true,&a);//定义一个QHotkey *对象，设置快捷键为config里的快捷键，全局可用。此时就成功注册快捷键了，也就是说按下快捷键会发出信号 //这句代码里已经把a作为父对象传给了hotkey，a会自动管理其内存，不需要手动释放内存
    //设置按下全局快捷键后会怎样
    QObject::connect(hotkey,&QHotkey::activated,
                     [&](){
                         chuangkou.move(config["chuangkou_x"].toInt(),config["chuangkou_y"].toInt());//把chuangkou移动到记录的位置
                         xianshi(chuangkou);
                     }
                    );
    //使用自定义的事件过滤器类HotkeyEditFilter，用于拦截hotkeyEdit的焦点事件，实现：1.当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复；2.当hotkeyEdit获得焦点时立即禁用当前已注册的全局快捷键；3.当hotkeyEdit失去焦点时判断用户输入的快捷键是否合规，合规的话就应用，不合规的话就恢复输入框为原始快捷键、弹出警告对话框
    hotkeyEdit.installEventFilter(   new HotkeyEditFilter(&hotkeyEdit,hotkey,configPath,itemHotkeys,&a)   );//创建事件过滤器对象，并把它安装到hotkeyEdit上

    //主窗口始终置顶设置
    QWidget zhidingWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout zhidingLayout(&zhidingWidget);//创建一个水平布局，放置在刚才创建的容器中。整这么麻烦是因为不这么做标签和复选框就上对齐，看起来不平行了
    zhidingLayout.setSpacing(4);//控件之间间距4像素
    zhidingLayout.setContentsMargins(0,0,0,0);//去掉布局的默认边距
    QCheckBox zhidingCheck(&zhidingWidget);//创建一个复选框
    zhidingCheck.setChecked(config["zhiding"].toBool());//读取全局对象config里的zhiding的值，然后显示在复选框里
    zhidingWidget.setFixedHeight(37);//通过给容器设置固定填充高度的方式，实现标签和复选框对齐
    zhidingLayout.addWidget(&zhidingCheck);//加入布局
    zhidingLayout.addStretch();//让水平布局右边控件整体靠左对齐
    formLayout->addRow("主窗口始终置顶：",&zhidingWidget);//在表单布局中添加一行，左边是标签“主窗口始终置顶：”，右边是复选框zhidingCheck
    if(config["zhiding"].toBool()==true){
        chuangkou.setWindowFlags( chuangkou.windowFlags() | Qt::WindowStaysOnTopHint );//在不改变其他窗口属性的前提下，给主窗口添加始终置顶属性
    }
    //切换主窗口始终置顶复选框触发
    QObject::connect(&zhidingCheck,&QCheckBox::toggled,
                     [&](bool checked){ //checked表示复选框的新状态，true表示勾选，false表示未勾选
                         if(checked){ //如果复选框勾上了
                             config["zhiding"]=true;
                             chuangkou.setWindowFlags( chuangkou.windowFlags() | Qt::WindowStaysOnTopHint );//在不改变其他窗口属性的前提下，给主窗口添加始终置顶属性
                             chuangkou.move(config["chuangkou_x"].toInt(),config["chuangkou_y"].toInt());//因为修改窗口属性后窗口会自动关闭，所以我们这里要手动显示主窗口
                             xianshi(chuangkou);
                             saveConfig(configPath);
                         }
                         else{ //如果复选框取消勾选
                             config["zhiding"]=false;
                             chuangkou.setWindowFlags( chuangkou.windowFlags() & ~Qt::WindowStaysOnTopHint );//在不改变其他窗口属性的前提下，给主窗口删除始终置顶属性
                             chuangkou.move(config["chuangkou_x"].toInt(),config["chuangkou_y"].toInt());
                             xianshi(chuangkou);
                             saveConfig(configPath);
                         }
                     }
                    );

    //输入时也将短语复制到剪贴板设置【【【【【现在这个设置是无效的，不管开不开对结果都没影响，因此之后我需要来实现一下
    QWidget clipboardWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout clipboardLayout(&clipboardWidget);//创建一个水平布局，放置在刚才创建的容器中。整这么麻烦是因为不这么做标签和复选框就上对齐，看起来不平行了
    clipboardLayout.setSpacing(4);//控件之间间距4像素
    clipboardLayout.setContentsMargins(0,0,0,0);//去掉布局的默认边距
    QCheckBox clipboardCheck(&clipboardWidget);//创建一个复选框
    clipboardCheck.setChecked(config["clipboard"].toBool());//读取全局对象config里的clipboard的值，然后显示在复选框里
    clipboardWidget.setFixedHeight(37);//通过给容器设置固定填充高度的方式，实现标签和复选框对齐
    clipboardLayout.addWidget(&clipboardCheck);//加入布局
    clipboardLayout.addStretch();//让水平布局右边控件整体靠左对齐
    formLayout->addRow("输入时也将短语复制到剪贴板：",&clipboardWidget);//在表单布局中添加一行，左边是标签“输入时也将短语复制到剪贴板：”，右边是复选框clipboardCheck
    //切换输入时也将短语复制到剪贴板复选框触发
    QObject::connect(&clipboardCheck,&QCheckBox::toggled,
                     [&](bool checked){ //checked表示复选框的新状态，true表示勾选，false表示未勾选
                         if(checked){ //如果复选框勾上了
                             config["clipboard"]=true;
                             saveConfig(configPath);
                         }
                         else{ //如果复选框取消勾选
                             config["clipboard"]=false;
                             saveConfig(configPath);
                         }
                     }
                    );

    //延迟多少毫秒再输入？设置【【【【【改一下这个名称
    QSpinBox delaySpin(&shezhichuangkou);//创建一个数字输入框
    delaySpin.setRange(0,2000);//设置输入范围为0~2000
    delaySpin.setValue(config["delay"].toInt());//读取全局对象config里的delay的值，然后显示在输入框里
    delaySpin.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);//让QSpinBox高度和标签对齐
    formLayout->addRow("延迟多少毫秒再输入？",&delaySpin);//在表单布局中添加一行，左边是标签“延迟多少毫秒再输入？”，右边是数字输入框delaySpin
    //如果用户修改了延迟多少毫秒再输入？
    QObject::connect(&delaySpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int value){
                         config["delay"]=value;
                         saveConfig(configPath);
                     }
                    );

    //窗口大小设置
    QWidget * sizeWidget=new QWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout * sizeLayout=new QHBoxLayout(sizeWidget);//创建一个水平布局，放置在刚才创建的容器中
    sizeLayout->setSpacing(4);//控件之间间距4像素
    sizeLayout->setContentsMargins(0,0,0,0);//去掉布局的默认边距
    sizeLayout->addWidget(new QLabel("宽度",sizeWidget));//加入布局
    QSpinBox * widthSpin=new QSpinBox(sizeWidget);//创建宽度输入框
    widthSpin->setRange(250,2000);//限制宽度输入范围250~2000
    widthSpin->setValue(config["width"].toInt());//读取全局对象config里的宽度，然后显示在输入框里
    widthSpin->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);//让QSpinBox高度和标签对齐
    sizeLayout->addWidget(widthSpin);//加入布局
    sizeLayout->addWidget(new QLabel("高度",sizeWidget));//加入布局
    QSpinBox * heightSpin=new QSpinBox(sizeWidget);//创建高度输入框
    heightSpin->setRange(250,2000);//限制高度输入范围250~2000
    heightSpin->setValue(config["height"].toInt());//读取全局对象config里的高度，然后显示在输入框里
    heightSpin->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);//让QSpinBox高度和标签对齐
    sizeLayout->addWidget(heightSpin);//加入布局
    sizeLayout->addStretch();//让水平布局右边控件整体靠左对齐
    formLayout->addRow("窗口大小：",sizeWidget);//在表单布局中添加一行，左边是标签“窗口大小：”，右边是“宽度”“高度”和两个输入框
    //如果用户在设置-窗口大小里修改了宽度/高度，那么写入程序设置到config.json，同时调整所有窗口大小，这个功能的实现代码我放最后面了

    //默认短语项高度设置
    QSpinBox itemHeightSpin(&shezhichuangkou);//创建一个数字输入框
    itemHeightSpin.setRange(10,100);//设置输入范围为10~100像素【【【【【
    itemHeightSpin.setValue(config["default_item_height"].toInt());//读取全局对象config里的default_item_height的值，然后显示在输入框里
    itemHeightSpin.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);//让QSpinBox高度和标签对齐
    formLayout->addRow("默认短语项高度：",&itemHeightSpin);//在表单布局中添加一行，左边是标签“默认短语项高度：”，右边是数字输入框itemHeightSpin
    //如果用户修改了默认短语项高度
    QObject::connect(&itemHeightSpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int value){
                         config["default_item_height"]=value;
                         saveConfig(configPath);
                     }
                    );

    //滚动条滚动速度设置
    QSpinBox gundongSpin(&shezhichuangkou);//创建一个数字输入框
    gundongSpin.setRange(1,100);//设置输入范围为1~100
    gundongSpin.setValue(config["gundong"].toInt());//读取全局对象config里的gundong的值，然后显示在输入框里
    gundongSpin.setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);//让QSpinBox高度和标签对齐
    formLayout->addRow("滚动条滚动速度：",&gundongSpin);//在表单布局中添加一行，左边是标签“滚动条滚动速度：”，右边是数字输入框gundongSpin
    //如果用户修改了滚动条滚动速度
    QObject::connect(&gundongSpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int value){
                         config["gundong"]=value;
                         saveConfig(configPath);
                     }
                    );

    //角标放左上角还是右上角？设置
    QWidget jiaobiaoWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout jiaobiaoLayout(&jiaobiaoWidget);//创建一个水平布局，放置在刚才创建的容器中。整这么麻烦是因为不这么做标签和复选框就上对齐，看起来不平行了
    jiaobiaoLayout.setSpacing(4);//控件之间间距4像素
    jiaobiaoLayout.setContentsMargins(0,0,0,0);//去掉布局的默认边距
    QCheckBox jiaobiaoCheck(&jiaobiaoWidget);//创建一个复选框
    jiaobiaoCheck.setChecked(config["jiaobiao"].toBool());//读取全局对象config里的jiaobiao的值，然后显示在复选框里
    jiaobiaoWidget.setFixedHeight(37);//通过给容器设置固定填充高度的方式，实现标签和复选框对齐
    jiaobiaoLayout.addWidget(&jiaobiaoCheck);//加入布局
    jiaobiaoLayout.addStretch();//让水平布局右边控件整体靠左对齐
    formLayout->addRow("角标放左上角还是右上角？",&jiaobiaoWidget);//在表单布局中添加一行，左边是标签“角标放左上角还是右上角？”，右边是复选框jiaobiaoCheck
    //切换角标放左上角还是右上角？复选框触发
    QObject::connect(&jiaobiaoCheck,&QCheckBox::toggled,
                     [&](bool checked){ //checked表示复选框的新状态，true表示勾选，false表示未勾选
                         if(checked){ //如果复选框勾上了
                             config["jiaobiao"]=true;
                             saveConfig(configPath);
                             QMessageBox::information(&shezhichuangkou,"提示","鼠标移到主窗口后生效  ");
                         }
                         else{ //如果复选框取消勾选
                             config["jiaobiao"]=false;
                             saveConfig(configPath);
                             QMessageBox::information(&shezhichuangkou,"提示","鼠标移到主窗口后生效  ");
                         }
                     }
                    );

    //开机自启动设置
    QWidget * autostartupWidget=new QWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout * autostartupLayout=new QHBoxLayout(autostartupWidget);//创建一个水平布局，放置在刚才创建的容器中。整这么麻烦是因为不这么做标签和复选框就上对齐，看起来不平行了
    autostartupLayout->setSpacing(4);//控件之间间距4像素
    autostartupLayout->setContentsMargins(0,0,0,0);//去掉布局的默认边距
    QCheckBox * autostartupCheck=new QCheckBox(autostartupWidget);//创建一个复选框
    autostartupCheck->setChecked(config["ziqidong"].toBool());//读取全局对象config里的自启动的值，然后显示在复选框里
    autostartupWidget->setFixedHeight(37);//通过给容器设置固定填充高度的方式，实现标签和复选框对齐
    autostartupLayout->addWidget(autostartupCheck);//加入布局
    autostartupLayout->addStretch();//让水平布局右边控件整体靠左对齐
    formLayout->addRow("开机自启动：",autostartupWidget);//在表单布局中添加一行，左边是标签“开机自启动：”，右边是复选框autostartupCheck
    QString autostartupRegPath="HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";//注册表Run项路径
    QString autostartupRegName="QuickSay";//取“QuickSay”作为注册表键名
    if(config["ziqidong"].toBool()==true){
        QSettings reg(autostartupRegPath,QSettings::NativeFormat);//创建QSettings对象，用于访问注册表Run项
        if(   reg.value(autostartupRegName).toString()   !=   QString("\"%1\" %2").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())).arg("--autostart")   ){ //获取注册表Run项里的这个键名对应的值（放心，考虑了键不存在的情况），如果值不等于可执行文件的完整路径（调整后） //QCoreApplication::applicationFilePath()返回的是可执行文件的完整路径（包含文件名和扩展名）
            reg.setValue(autostartupRegName,   QString("\"%1\" %2").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())).arg("--autostart")   );//那么在注册表Run项里写入这个键，并将可执行文件的完整路径（调整后）设置为这个键对应的值。于是实现开机自启动
        }
    }
    //切换开机自启动复选框触发
    QObject::connect(autostartupCheck,&QCheckBox::toggled,
                     [&](bool checked){
                         QSettings reg(autostartupRegPath,QSettings::NativeFormat);//创建QSettings对象，用于访问注册表Run项
                         if(checked){ //如果复选框勾上了
                             reg.setValue(autostartupRegName,   QString("\"%1\" %2").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())).arg("--autostart")   );//那么在注册表Run项里写入这个键，并将可执行文件的完整路径（调整后）设置为这个键对应的值。于是实现开机自启动
                             config["ziqidong"]=true;
                             saveConfig(configPath);//写入程序设置到config.json
                         }
                         else{ //如果复选框取消勾选
                             reg.remove(autostartupRegName);//那么在注册表Run项里移除这个键。于是取消开机自启动
                             config["ziqidong"]=false;
                             saveConfig(configPath);//写入程序设置到config.json
                         }
                     }
                    );

    //版本号显示
    QWidget versionWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout versionLayout(&versionWidget);//创建一个水平布局，放置在刚才创建的容器中
    versionLayout.setSpacing(4);//控件之间间距4像素
    versionLayout.setContentsMargins(0,0,0,0);//去掉布局的默认边距
    QPushButton versionButton("1.6.0",&versionWidget);//创建版本号按钮 //【【【更新版本后记得改一下这里的文本】】】
    versionButton.setFixedWidth(100);//固定版本号按钮的宽度为100像素
    versionButton.setCursor(Qt::PointingHandCursor);//当鼠标悬停在该按钮上时，鼠标光标变成手形状
    versionWidget.setFixedHeight(37);//通过给容器设置固定填充高度的方式，实现标签和复选框对齐
    versionLayout.addWidget(&versionButton);//加入布局
    versionLayout.addStretch();//让水平布局右边控件整体靠左对齐
    formLayout->addRow("版本：",&versionWidget);//在表单布局中添加一行，左边是标签“版本：”，右边是版本号按钮versionButton
    //当用户点击版本号按钮时触发
    QObject::connect(&versionButton,&QPushButton::clicked,
                     [](){
                         QDesktopServices::openUrl(QUrl("https://github.com/DarkKandaoMaster/QuickSay"));//调用QDesktopServices类，使用系统默认浏览器打开指定网址
                        //  QDesktopServices::openUrl(QUrl("https://www.52pojie.cn/thread-2061920-1-1.html"));
                     }
                    );

    //在chuangkou右上角放一个“设置”按钮
    QPushButton shezhi("",&chuangkou);//创建设置按钮，文本为空字符串。不然文本也会显示在按钮上
    shezhi.setObjectName("iconButton");//应用图标按钮样式
    shezhi.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/settings.svg"));//设置按钮图标
    shezhi.setIconSize(QSize(20,20));//调整图标大小为20*20像素
    shezhi.setToolTip("设置");//设置鼠标悬停提示文字为“设置”
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip,false);//禁用Qt自带的QToolTip悬停提示动画
    //当按下“设置”按钮时，弹出shezhichuangkou窗口
    QObject::connect(&shezhi,&QPushButton::clicked,
                     [&](){
                         shezhichuangkou.move(config["shezhichuangkou_x"].toInt(),config["shezhichuangkou_y"].toInt());//把shezhichuangkou移动到记录的位置
                         xianshi(shezhichuangkou);
                     }
                    );

    //创建tianjiachuangkou窗口
    QString currentTabName="";//记录用户点击右上角加号时当前标签的名称
    QWidget tianjiachuangkou;
    g_tianjiachuangkou=&tianjiachuangkou;
    tianjiachuangkou.setWindowTitle("QuickSay-添加");
    tianjiachuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QPlainTextEdit tianjiakuang(&tianjiachuangkou);
    QPushButton tianjia_gaojishuru("如何更高级地输入？",&tianjiachuangkou);
    QLabel tianjia_beizhuwenben("备注：",&tianjiachuangkou);
    QPlainTextEdit tianjia_beizhukuang(&tianjiachuangkou);
    QLabel tianjia_kjjwenben("快捷键：",&tianjiachuangkou);
    QKeySequenceEdit tianjia_kjjkuang(&tianjiachuangkou);
    QPushButton tianjia_kjjqingkong("清空",&tianjiachuangkou);
    QPushButton tianjiaquxiao("取消",&tianjiachuangkou);
    QPushButton tianjiaqueding("添加",&tianjiachuangkou);
    //当按下“如何更高级地输入？”按钮时，弹出表达式提示窗口
    QObject::connect(&tianjia_gaojishuru,&QPushButton::clicked,
                     [&](){
                         showAdvancedInputHelp(tianjiachuangkou);
                     }
                    );
    //当按下“清空”按钮时，直接清空快捷键输入框
    QObject::connect(&tianjia_kjjqingkong,&QPushButton::clicked,
                     [&](){
                         tianjia_kjjkuang.clear();
                     }
                    );
    //当按下“取消”按钮时，直接关闭tianjiachuangkou窗口
    QObject::connect(&tianjiaquxiao,&QPushButton::clicked,
                     [&](){
                         tianjiachuangkou.close();
                     }
                    );
    //当按下“添加”按钮时
    QObject::connect(&tianjiaqueding,&QPushButton::clicked,
                     [&](){
                         QKeySequence seq=tianjia_kjjkuang.keySequence();//获取快捷键输入框里的快捷键
                         if(!seq.isEmpty()){ //如果快捷键不为空
                             if(!isValidHotkey(seq,itemHotkeys,&tianjia_kjjkuang)){ //如果快捷键不合规（调用isValidHotkey函数检查快捷键是否合规）
                                 tianjia_kjjkuang.clear();//清空快捷键输入框
                                 return ;//结束当前这个槽函数的执行，但是不关闭tianjiachuangkou，于是用户可以重新在这个窗口输入快捷键
                             }
                         }

                         QString text=tianjiakuang.toPlainText();//获取输入框里的内容
                         if(!text.isEmpty()){ //如果获取到的内容不是空的
                             QListWidgetItem * newItem=new QListWidgetItem();//new一个短语项对象，并把它的地址给newItem
                             newItem->setData(Qt::UserRole,text);//把用户输入的短语存到newItem的Qt::UserRole
                             newItem->setData(Qt::UserRole+1,tianjia_beizhukuang.toPlainText());//获取备注输入框里的内容，并把输入内容存到对应短语项的Qt::UserRole+1
                             newItem->setData(Qt::UserRole+2,currentTabName);//把记录的标签名称存到newItem的Qt::UserRole+2
                             newItem->setData(Qt::UserRole+3,seq.toString());//把用户输入的快捷键字符串存到对应短语项的Qt::UserRole+3
                             updateItemDisplay(newItem);//更新对应短语项的显示
                             liebiao.addItem(newItem);//把newItem添加到列表
                             saveListToJson(liebiao,dataPath);//添加后写入列表内容到data.json
                             rebuildItemHotkeys(liebiao,itemHotkeys,&a);//添加后为liebiao中的短语项注册快捷键
                             filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),search.text());//添加后根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                         }
                         tianjiachuangkou.close();
                     }
                    );
    //使用自定义的事件过滤器类KjjHotkeyEditFilter，用于拦截tianjia_kjjkuang的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
    tianjia_kjjkuang.installEventFilter(   new KjjHotkeyEditFilter(&tianjia_kjjkuang,itemHotkeys,&a)   );//创建事件过滤器对象，并把它安装到tianjia_kjjkuang上

    //在chuangkou右上角放一个“添加”按钮
    QPushButton tianjia("",&chuangkou);//创建添加按钮，文本为空字符串
    tianjia.setObjectName("iconButton");//应用图标按钮样式
    tianjia.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/add.svg"));//设置按钮图标
    tianjia.setIconSize(QSize(20,20));//调整图标大小为20*20像素
    tianjia.setToolTip("添加短语");//设置鼠标悬停提示文字为“添加短语”
    //当按下“添加”按钮时
    QObject::connect(&tianjia,&QPushButton::clicked,
                     [&](){
                         tianjiakuang.clear();
                         tianjia_beizhukuang.clear();
                         tianjia_kjjkuang.clear();
                         currentTabName=tabBar.tabText(tabBar.currentIndex());//记录用户点击右上角加号时当前标签的名称
                         tianjiachuangkou.move(config["tianjiachuangkou_x"].toInt(),config["tianjiachuangkou_y"].toInt());//把tianjiachuangkou移动到记录的位置
                         xianshi(tianjiachuangkou);
                         tianjiakuang.setFocus();//把焦点给到tianjiakuang，而不是其他控件
                     }
                    );

    //在chuangkou右上角放一个图钉按钮
    QPushButton tuding("",&chuangkou);//创建图钉按钮，文本为空字符串
    tuding.setObjectName("iconButton");//应用图标按钮样式
    if(config["tudingflag"].toBool()==true){ //如果钉住窗口
        tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/实心图钉.svg"));//设置按钮图标为实心图钉
        tuding.setToolTip("已开启连续输入");//设置鼠标悬停提示文字为“已开启连续输入”
    }
    else{ //如果没有钉住窗口
        tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/空心图钉.svg"));//设置按钮图标为空心图钉
        tuding.setToolTip("已关闭连续输入");//设置鼠标悬停提示文字为“已关闭连续输入”
    }
    tuding.setIconSize(QSize(20,20));//调整图标大小为20*20像素
    //当按下图钉按钮时，切换按钮图标
    QObject::connect(&tuding,&QPushButton::clicked,
                     [&](){
                         if(config["tudingflag"].toBool()==true){ //如果钉住窗口
                             tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/空心图钉.svg"));//切换按钮图标为空心图钉
                             tuding.setToolTip("已关闭连续输入");//设置鼠标悬停提示文字为“已关闭连续输入”
                             config["tudingflag"]=false;
                             saveConfig(configPath);//写入程序设置到config.json
                         }
                         else{ //如果没有钉住窗口
                             tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/实心图钉.svg"));//切换按钮图标为实心图钉
                             tuding.setToolTip("已开启连续输入");//设置鼠标悬停提示文字为“已开启连续输入”
                             config["tudingflag"]=true;
                             saveConfig(configPath);//写入程序设置到config.json
                         }
                     }
                    );

    //创建xiugaichuangkou窗口
    QListWidgetItem * currentEditingItem=nullptr;//记录用户点到的是liebiao中的哪个选项
    QWidget xiugaichuangkou;
    g_xiugaichuangkou=&xiugaichuangkou;
    xiugaichuangkou.setWindowTitle("QuickSay-修改");
    xiugaichuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QPlainTextEdit xiugaikuang(&xiugaichuangkou);
    QPushButton xiugai_gaojishuru("如何更高级地输入？",&xiugaichuangkou);
    QLabel xiugai_beizhuwenben("备注：",&xiugaichuangkou);
    QPlainTextEdit xiugai_beizhukuang(&xiugaichuangkou);
    QLabel xiugai_kjjwenben("快捷键：",&xiugaichuangkou);
    QKeySequenceEdit xiugai_kjjkuang(&xiugaichuangkou);
    QPushButton xiugai_kjjqingkong("清空",&xiugaichuangkou);
    QPushButton xiugaiquxiao("取消",&xiugaichuangkou);
    QPushButton xiugaiqueding("修改",&xiugaichuangkou);
    //当按下“如何更高级地输入？”按钮时，弹出表达式提示窗口
    QObject::connect(&xiugai_gaojishuru,&QPushButton::clicked,
                     [&](){
                         showAdvancedInputHelp(xiugaichuangkou);
                     }
                    );
    //当按下“清空”按钮时，直接清空快捷键输入框
    QObject::connect(&xiugai_kjjqingkong,&QPushButton::clicked,
                     [&](){
                         xiugai_kjjkuang.clear();
                     }
                    );
    //当按下“取消”按钮时，直接关闭xiugaichuangkou窗口
    QObject::connect(&xiugaiquxiao,&QPushButton::clicked,
                     [&](){
                         xiugaichuangkou.close();
                     }
                    );
    //当按下“修改”按钮时
    QObject::connect(&xiugaiqueding,&QPushButton::clicked,
                     [&](){
                         if(!currentEditingItem){ //如果currentEditingItem为空指针 //以防万一用
                             xiugaichuangkou.close();//关闭xiugaichuangkou
                             return ;//结束当前这个槽函数的执行
                         }

                         QKeySequence seq=xiugai_kjjkuang.keySequence();//获取快捷键输入框里的快捷键
                         if(!seq.isEmpty()){ //如果快捷键不为空
                             if(!isValidHotkey(seq,itemHotkeys,&xiugai_kjjkuang,   itemHotkeys[   liebiao.row(currentEditingItem)   ])   ){ //如果快捷键不合规（调用isValidHotkey函数检查快捷键是否合规） //这里需要传入selfhk参数，即最后一个参数，用来防止自己和自己冲突 //我来解释一下最后一个参数哈：它先是获取当前正在编辑的短语项的行号，然后取出itemHotkeys数组中对应行号的QHotkey *指针，如果该短语项没有快捷键，那么取出的就是nullptr
                                 xiugai_kjjkuang.setKeySequence( QKeySequence(currentEditingItem->data(Qt::UserRole+3).toString()) );//把对应短语项的Qt::UserRole+3里的快捷键字符串重新放进输入框，也就是说恢复输入框为原始快捷键
                                 return ;//结束当前这个槽函数的执行，但是不关闭xiugaichuangkou，于是用户可以重新在这个窗口输入快捷键
                             }
                         }
                         currentEditingItem->setData(Qt::UserRole+3,seq.toString());//如果快捷键合规或者为空，那么把用户输入的快捷键字符串存到对应短语项的Qt::UserRole+3

                         QString text=xiugaikuang.toPlainText();//获取输入框里的内容
                         if(!text.isEmpty()){ //如果获取到的内容不是空的
                             currentEditingItem->setData(Qt::UserRole,text);//把输入内容修改到短语项的Qt::UserRole中
                         }

                         currentEditingItem->setData(Qt::UserRole+1,xiugai_beizhukuang.toPlainText());//获取备注输入框里的内容，并把输入内容存到对应短语项的Qt::UserRole+1

                         updateItemDisplay(currentEditingItem);//更新对应短语项的显示
                         saveListToJson(liebiao,dataPath);//修改后写入列表内容到data.json
                         rebuildItemHotkeys(liebiao,itemHotkeys,&a);//修改后为liebiao中的短语项注册快捷键
                         filterListByTab(liebiao,tabBar.tabText(tabBar.currentIndex()),search.text());//修改后根据当前选中标签和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                         xiugaichuangkou.close();
                     }
                    );
    //使用自定义的事件过滤器类KjjHotkeyEditFilter，用于拦截xiugai_kjjkuang的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
    xiugai_kjjkuang.installEventFilter(   new KjjHotkeyEditFilter(&xiugai_kjjkuang,itemHotkeys,&a)   );//创建事件过滤器对象，并把它安装到xiugai_kjjkuang上

    //右键liebiao中的某个选项时，弹出一个菜单，上面有修改、删除两个选项
    QMenu menu1;
    QAction xiugai("修改",&menu1);
    menu1.addAction(&xiugai);
    QAction shanchu("删除",&menu1);
    menu1.addAction(&shanchu);
    liebiao.setContextMenuPolicy(Qt::CustomContextMenu);//为liebiao设置自定义右键菜单
    //当右键liebiao时，执行lambda表达式
    QObject::connect(&liebiao,&QListWidget::customContextMenuRequested,
                     [&](const QPoint & pos){
                         QListWidgetItem * item=liebiao.itemAt(pos);//根据点击位置，找到对应的QListWidgetItem（如果点到空白区域则返回空指针）
                         if(item){ //如果点到liebiao中的某个选项，那么弹出菜单，上面有修改、删除两个选项
                             QAction * selectedAction=menu1.exec(liebiao.mapToGlobal(pos));//在鼠标点击的位置弹出菜单，等待用户选择一个QAction
                             if(selectedAction==&xiugai){ //如果用户选了“修改”
                                 xiugaikuang.clear();
                                 xiugai_beizhukuang.clear();
                                 xiugai_kjjkuang.clear();
                                 currentEditingItem=item;//记录当前要修改的选项
                                 xiugaikuang.setPlainText(item->data(Qt::UserRole).toString());//把原短语放进输入框
                                 xiugai_beizhukuang.setPlainText(item->data(Qt::UserRole+1).toString());//把原备注放进输入框
                                 xiugai_kjjkuang.setKeySequence( QKeySequence(item->data(Qt::UserRole+3).toString()) );//把这个短语项的Qt::UserRole+3里的快捷键字符串放进输入框
                                 xiugaichuangkou.move(config["xiugaichuangkou_x"].toInt(),config["xiugaichuangkou_y"].toInt());//把xiugaichuangkou移动到记录的位置
                                 xianshi(xiugaichuangkou);
                                 xiugaikuang.setFocus();//把焦点给到xiugaikuang，而不是其他控件
                                 xiugaikuang.selectAll();//全选输入框里的所有文本
                             }
                             else if(selectedAction==&shanchu){ //如果用户选了“删除”
                                 delete item;
                                 saveListToJson(liebiao,dataPath);//删除后写入列表内容到data.json
                                 rebuildItemHotkeys(liebiao,itemHotkeys,&a);//删除后为liebiao中的短语项注册快捷键
                             }
                         }
                     }
                    );

    //创建托盘
    QSystemTrayIcon * trayIcon=new QSystemTrayIcon(&a);
    trayIcon->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));//设置托盘图标
    //左键托盘会怎样
    //托盘被激活时触发（比如左键右键点击托盘）
    QObject::connect(trayIcon,&QSystemTrayIcon::activated,
                     [&](QSystemTrayIcon::ActivationReason reason){ //reason变量可以用来接收托盘被激活的具体原因
                         if(reason==QSystemTrayIcon::Trigger){ //如果具体原因是鼠标左键单击
                             chuangkou.move(config["chuangkou_x"].toInt(),config["chuangkou_y"].toInt());
                             xianshi(chuangkou);
                         }
                     }
                    );
    //右键托盘会怎样
    //给托盘创建一个右键菜单
    QMenu * menu=new QMenu();
    QAction * shezhiAction=new QAction("设置",menu);//添加“设置”菜单项
    menu->addAction(shezhiAction);//shezhiAction的内存不用释放，因为此时menu会接管shezhiAction的所有权，自动管理其内存
    QObject::connect(shezhiAction,&QAction::triggered,
                     [&](){ //当按下“设置”菜单项时，弹出shezhichuangkou窗口
                         shezhichuangkou.move(config["shezhichuangkou_x"].toInt(),config["shezhichuangkou_y"].toInt());//把shezhichuangkou移动到记录的位置
                         xianshi(shezhichuangkou);
                     }
                    );
    QAction * quitAction=new QAction("退出",menu);//添加“退出”菜单项
    menu->addAction(quitAction);//quitAction的内存也不用释放，因为此时menu会接管quitAction的所有权，自动管理其内存
    QObject::connect(quitAction,&QAction::triggered,&a,&QApplication::quit);
    trayIcon->setContextMenu(menu);//menu的内存也不用释放，因为此时trayIcon会接管menu的所有权，自动管理其内存
    trayIcon->show();
    trayIcon->setToolTip("QuickSay");//设置鼠标悬停在托盘上时显示的提示文字。这句代码必须写在show()之后

    //使用自定义的事件过滤器类WindowMoveFilter，实现：当用户移动窗口时记录窗口位置。使得呼出窗口时让窗口在记录的位置显示
    a.installEventFilter(   new WindowMoveFilter(&chuangkou,&shezhichuangkou,&tianjiachuangkou,&xiugaichuangkou,configPath,&a)   );//创建事件过滤器对象，并把它安装到a上
    //使用自定义的事件过滤器类MyEventFilter，实现：Esc键可以关闭主窗口/添加窗口/修改窗口；回车键Enter可以输出光标处短语；左右方向键可以切换标签
    a.installEventFilter(   new MyEventFilter(&chuangkou,&tianjiachuangkou,&xiugaichuangkou,&tianjiaquxiao,&xiugaiquxiao,&liebiao,&tabBar,&search)   );//创建事件过滤器对象，并把它安装到a上

    //从全局对象config读取窗口大小，然后根据宽高，设置所有窗口大小和所有控件大小，以及所有控件位置
    adjustAllWindows(config["width"].toInt(),config["height"].toInt(),
                     chuangkou,liebiao,tabBar,search,shezhi,tianjia,tuding,
                     shezhichuangkou,
                     tianjiachuangkou,tianjiakuang,tianjia_gaojishuru,tianjia_beizhuwenben,tianjia_beizhukuang,tianjia_kjjwenben,tianjia_kjjkuang,tianjia_kjjqingkong,tianjiaquxiao,tianjiaqueding,
                     xiugaichuangkou,xiugaikuang,xiugai_gaojishuru,xiugai_beizhuwenben,xiugai_beizhukuang,xiugai_kjjwenben,xiugai_kjjkuang,xiugai_kjjqingkong,xiugaiquxiao,xiugaiqueding
                    );
    //如果用户在设置-窗口大小里修改了宽度，那么写入程序设置到config.json，同时调整所有窗口大小和所有控件大小，以及所有控件位置
    QObject::connect(widthSpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int w){
                         config["width"]=w;//更新config里的宽度
                         config["height"]=heightSpin->value();//读取当前高度，然后更新config里的高度
                         saveConfig(configPath);//写入程序设置到config.json
                         //调用adjustAllWindows函数，调整所有窗口大小
                         adjustAllWindows(config["width"].toInt(),config["height"].toInt(),
                                          chuangkou,liebiao,tabBar,search,shezhi,tianjia,tuding,
                                          shezhichuangkou,
                                          tianjiachuangkou,tianjiakuang,tianjia_gaojishuru,tianjia_beizhuwenben,tianjia_beizhukuang,tianjia_kjjwenben,tianjia_kjjkuang,tianjia_kjjqingkong,tianjiaquxiao,tianjiaqueding,
                                          xiugaichuangkou,xiugaikuang,xiugai_gaojishuru,xiugai_beizhuwenben,xiugai_beizhukuang,xiugai_kjjwenben,xiugai_kjjkuang,xiugai_kjjqingkong,xiugaiquxiao,xiugaiqueding
                                         );
                     }
                    );
    //如果用户在设置-窗口大小里修改了高度，那么写入程序设置到config.json，同时调整所有窗口大小和所有控件大小，以及所有控件位置
    QObject::connect(heightSpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int h){
                         config["height"]=h;//更新config里的高度
                         config["width"]=widthSpin->value();//读取当前宽度，然后更新config里的宽度
                         saveConfig(configPath);//写入程序设置到config.json
                         //调用adjustAllWindows函数，调整所有窗口大小
                         adjustAllWindows(config["width"].toInt(),config["height"].toInt(),
                                          chuangkou,liebiao,tabBar,search,shezhi,tianjia,tuding,
                                          shezhichuangkou,
                                          tianjiachuangkou,tianjiakuang,tianjia_gaojishuru,tianjia_beizhuwenben,tianjia_beizhukuang,tianjia_kjjwenben,tianjia_kjjkuang,tianjia_kjjqingkong,tianjiaquxiao,tianjiaqueding,
                                          xiugaichuangkou,xiugaikuang,xiugai_gaojishuru,xiugai_beizhuwenben,xiugai_beizhukuang,xiugai_kjjwenben,xiugai_kjjkuang,xiugai_kjjqingkong,xiugaiquxiao,xiugaiqueding
                                         );
                     }
                    );

    if(!   a.arguments().contains("--autostart")   ){ //程序启动时检查程序启动参数，如果没有包含我们专门为开机自启添加的标记“--autostart”（也就是说用户是通过双击可执行文件打开的程序，而不是通过开机自启自动打开的程序）
        chuangkou.move(config["chuangkou_x"].toInt(),config["chuangkou_y"].toInt());//把chuangkou移动到记录的位置
        xianshi(chuangkou);
    }
    //否则就是通过开机自启自动打开的程序，那么什么也不做，就后台运行个托盘



    return a.exec();
}
