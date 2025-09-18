//更新内容：
//右键语录菜单新增了一个按钮“快捷键”，点它可以弹出一个设置语录快捷键的窗口，设置完后就能通过快捷键输入当前语录，而不必呼出界面选择

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
#ifdef _WIN32
#include<windows.h>
#pragma comment(lib,"user32.lib")
#endif

QJsonObject config;//全局对象，用于保存程序的设置

void saveConfig(const QString &configPath){ //写入程序设置到config.json
    QJsonDocument doc(config);//把全局对象config转换成JSON文档
    QFile file(configPath);//打开指定路径文件config.json
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)){ //如果config.json成功打开（写模式）
        file.write(doc.toJson());//写入JSON文档到本地
        file.close();//关闭文件
    }
}

void loadConfig(const QString &configPath){ //读取config.json到程序设置。如果config.json不存在，那么读取默认设置，同时写入默认设置到config.json
    QFile file(configPath);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)){ //如果config.json成功打开（读模式）
        QByteArray data=file.readAll();//把config.json所有内容读取到data
        file.close();//关闭文件
        QJsonDocument doc=QJsonDocument::fromJson(data);//把读取到的数据转换成JSON文档
        if(doc.isObject()){ //确认文档是一个对象
            config=doc.object();//取出JSON对象，把这个对象赋值给全局对象config
        }
    }
    else{ //如果config.json不存在
        config["hotkey"]="Ctrl+Shift+V";//默认全局快捷键【【【注：想修改默认设置在这里修改】】】
        config["width"]=500;//默认窗口宽度
        config["height"]=500;//默认窗口高度
        config["tudingflag"]=true;//默认钉住窗口，即已开启失去焦点不隐藏
        config["ziqidong"]=true;//默认开机自启动
        saveConfig(configPath);//写入默认设置到config.json
    }
}

void saveListToJson(QListWidget &liebiao,const QString &dataPath){ //写入列表内容到data.json
    QJsonArray jsonArray;//创建一个JSON数组
    for(int i=0;i<liebiao.count();i++){ //遍历列表中的所有项
        QJsonObject obj;//创建一个JSON对象
        obj["text"]=liebiao.item(i)->text();//把当前项的文本存到对象的"text"字段
        obj["hotkey"]=liebiao.item(i)->data(Qt::UserRole).toString();//把用户设置的当前项对应的快捷键字符串存到对象的"hotkey"字段，如果没有设置则为空字符串
        jsonArray.append(obj);//把对象加入数组
    }
    QJsonDocument doc(jsonArray);//把数组包装成JSON文档
    QFile file(dataPath);//打开指定路径的文件
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)){ //如果文件成功打开（写模式）
        file.write(doc.toJson());//把JSON文档写入文件
        file.close();//关闭文件
    }
}

void loadListFromJson(QListWidget &liebiao,const QString &dataPath){ //读取data.json到列表内容。如果data.json不存在，那么读取默认列表内容（也就是新手教程），同时写入默认列表内容到data.json
    QFile file(dataPath);//打开指定路径的文件
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)){ //如果文件成功打开（读模式）
        QByteArray data=file.readAll();//把文件内容读到内存
        file.close();//关闭文件
        QJsonDocument doc=QJsonDocument::fromJson(data);//把数据解析成JSON文档
        if(doc.isArray()){ //确认文档是一个数组
            liebiao.clear();//清空当前列表
            QJsonArray jsonArray=doc.array();//取出JSON数组
            for(auto value:jsonArray){ //遍历数组中的每个元素
                if(value.isObject()){ //确认元素是对象
                    QJsonObject obj=value.toObject();//转换为对象
                    QString text=obj["text"].toString();//取出"text"字段
                    QString hotkeyStr=obj["hotkey"].toString();//取出"hotkey"字段
                    QListWidgetItem * it=new QListWidgetItem(text);//new一个带有文本text的列表项对象，并把它的地址给it。不直接liebiao.addItem(text)是因为这样就用不了Qt::UserRole了；使用new是因为这样它就不会在当前函数结束时被销毁
                    it->setData(Qt::UserRole,hotkeyStr);//把快捷键字符串保存到it的Qt::UserRole里
                    liebiao.addItem(it);//把it添加到列表。没错直接addItem指针是完全可以的 //it的内存不用释放，因为此时liebiao会接管it的所有权，自动管理其内存
                }
            }
        }
    }
    else{ //如果data.json不存在
        liebiao.addItem("快捷键：按下快捷键（默认Ctrl+Shift+V）呼出QuickSay\n点击语录：即可快速输入\n添加语录：点右上角加号\n修改/删除：右键语录\n排序：拖动语录");//【【【注：想修改默认列表内容（也就是新手教程）在这里修改】】】
        liebiao.addItem("OK！这些就是QuickSay的基本使用操作啦 (￣▽￣)~*\n想看全部操作的话请看“2·QuickSay全部使用操作.txt”");
        liebiao.addItem("感谢大家使用QuickSay！\n如果觉得好用的话还请去Github点个Star！拜托了！");
        liebiao.addItem("这里再放一个闲聊群💬：1026364290\n欢迎来玩！什么都可以聊哦 ヾ(≧▽≦*)o\n反馈建议的话，在这个群里@我或者私聊我，我回复得更快！\n如果在我能力范围内，马上修改，马上发布！");
        liebiao.addItem("感谢您能听我唠叨到这里！让我们开始吧！把这些语录都删掉，然后新建一个语录");
        saveListToJson(liebiao,dataPath);
    }
}

void pressKey(WORD vk){ //自定义一个函数，实现按下+抬起某个按键
    INPUT in[2]={};
    in[0].type=INPUT_KEYBOARD;in[0].ki.wVk=vk;
    in[1].type=INPUT_KEYBOARD;in[1].ki.wVk=vk;in[1].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(2,in,sizeof(INPUT));
}

void shuchu(const QListWidgetItem * item,QWidget * chuangkou){ //当按下liebiao中的某个选项时，这个选项里的文本就复制到剪贴板，然后chuangkou隐藏，然后模拟输入Ctrl+V
    QApplication::clipboard()->setText(item->text());//复制这个选项里的文本到剪贴板
    chuangkou->close();//隐藏窗口到托盘
    QTimer::singleShot(50, //延时个50毫秒再粘贴，这样才能成功在微信电脑版的输入框里输入
                       [](){
#ifdef _WIN32
                           //模拟输入Ctrl+V
                           //Ctrl键按下
                           INPUT ctrlDown={};ctrlDown.type=INPUT_KEYBOARD;ctrlDown.ki.wVk=VK_LCONTROL;//定义一个INPUT结构体用于发送合成的键事件；指定这是键盘事件；指定虚拟键码为左Ctrl键
                           SendInput(1,&ctrlDown,sizeof(INPUT));//发送合成的键事件（左Ctrl键按下）
                           //V键按下+抬起
                           pressKey('V');
                           //Ctrl键抬起
                           INPUT ctrlUp={};ctrlUp.type=INPUT_KEYBOARD;ctrlUp.ki.wVk=VK_LCONTROL;ctrlUp.ki.dwFlags=KEYEVENTF_KEYUP;//定义一个INPUT结构体用于发送合成的键事件；指定这是键盘事件；指定虚拟键码为左Ctrl键；指定这是键抬起事件
                           SendInput(1,&ctrlUp,sizeof(INPUT));//发送合成的键事件（左Ctrl键抬起）
#endif
                       }
                      );
}

void rebuildItemHotkeys(QListWidget & liebiao,QVector<QHotkey *> & itemHotkeys,QApplication * a){ //先禁用当前已注册的QHotkey *对象，然后遍历liebiao中的列表项，为它们注册快捷键
    for(auto hk:itemHotkeys){ //遍历动态数组中所有列表项对应的QHotkey *对象
        if(hk){ //如果该QHotkey *对象不是nullptr
            hk->setRegistered(false);//禁用当前已注册的这个QHotkey *对象
            delete hk;//释放这个QHotkey *对象的内存
        }
    }
    itemHotkeys.clear();//清空这个动态数组

    for(int i=0;i<liebiao.count();i++){ //遍历liebiao中的列表项
        QListWidgetItem * it=liebiao.item(i);//取出该列表项的指针，赋值给it
        QString hkStr=it->data(Qt::UserRole).toString();//返回保存在该列表项的Qt::UserRole里的快捷键字符串，用hkStr接收
        if(!hkStr.isEmpty()){ //如果hkStr不为空字符串
            QHotkey * hk=new QHotkey(QKeySequence(hkStr),true,a);//定义一个QHotkey *对象，设置快捷键为hkStr，全局可用。此时就成功注册快捷键了，也就是说按下快捷键会发出信号
            //设置按下快捷键后会怎样，即输出该列表项里的文本
            QObject::connect(hk,&QHotkey::activated,
                             [it](){
                                 QApplication::clipboard()->setText(it->text());//复制该列表项里的文本到剪贴板
#ifdef _WIN32
                                 //获取当前Ctrl、Alt、Shift、Meta键的物理按下状态，如果按下，那么合成对应键的抬起事件。不然模拟输入Ctrl+V时要出问题
                                 bool wasLCtrlDown=( GetAsyncKeyState(VK_LCONTROL) & 0x8000 )!=0;//获取当前左Ctrl键的物理按下状态
                                 bool wasRCtrlDown=( GetAsyncKeyState(VK_RCONTROL) & 0x8000 )!=0;//获取当前右Ctrl键的物理按下状态
                                 if(wasLCtrlDown){
                                     INPUT LCtrlUp={};LCtrlUp.type=INPUT_KEYBOARD;LCtrlUp.ki.wVk=VK_LCONTROL;LCtrlUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&LCtrlUp,sizeof(INPUT));
                                 }
                                 if(wasRCtrlDown){
                                     INPUT RCtrlUp={};RCtrlUp.type=INPUT_KEYBOARD;RCtrlUp.ki.wVk=VK_RCONTROL;RCtrlUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&RCtrlUp,sizeof(INPUT));
                                 }
                                 bool wasLAltDown=( GetAsyncKeyState(VK_LMENU) & 0x8000 )!=0;//获取当前左Alt键的物理按下状态，如果按下则为true
                                 bool wasRAltDown=( GetAsyncKeyState(VK_RMENU) & 0x8000 )!=0;//获取当前右Alt键的物理按下状态
                                 if(wasLAltDown){
                                     INPUT LAltUp={};LAltUp.type=INPUT_KEYBOARD;LAltUp.ki.wVk=VK_LMENU;LAltUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&LAltUp,sizeof(INPUT));
                                 }
                                 if(wasRAltDown){
                                     INPUT RAltUp={};RAltUp.type=INPUT_KEYBOARD;RAltUp.ki.wVk=VK_RMENU;RAltUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&RAltUp,sizeof(INPUT));
                                 }
                                 bool wasLShiftDown=( GetAsyncKeyState(VK_LSHIFT) & 0x8000 )!=0;//获取当前左Shift键的物理按下状态
                                 bool wasRShiftDown=( GetAsyncKeyState(VK_RSHIFT) & 0x8000 )!=0;//获取当前右Shift键的物理按下状态
                                 if(wasLShiftDown){
                                     INPUT LShiftUp={};LShiftUp.type=INPUT_KEYBOARD;LShiftUp.ki.wVk=VK_LSHIFT;LShiftUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&LShiftUp,sizeof(INPUT));
                                 }
                                 if(wasRShiftDown){
                                     INPUT RShiftUp={};RShiftUp.type=INPUT_KEYBOARD;RShiftUp.ki.wVk=VK_RSHIFT;RShiftUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&RShiftUp,sizeof(INPUT));
                                 }
                                 bool wasLMetaDown=( GetAsyncKeyState(VK_LWIN) & 0x8000 )!=0;//获取当前左Meta键的物理按下状态
                                 bool wasRMetaDown=( GetAsyncKeyState(VK_RWIN) & 0x8000 )!=0;//获取当前右Meta键的物理按下状态
                                 if(wasLMetaDown){
                                     INPUT LMetaUp={};LMetaUp.type=INPUT_KEYBOARD;LMetaUp.ki.wVk=VK_LWIN;LMetaUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&LMetaUp,sizeof(INPUT));
                                 }
                                 if(wasRMetaDown){
                                     INPUT RMetaUp={};RMetaUp.type=INPUT_KEYBOARD;RMetaUp.ki.wVk=VK_RWIN;RMetaUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                     SendInput(1,&RMetaUp,sizeof(INPUT));
                                 }
                                 //模拟输入Ctrl+V
                                 INPUT ctrlDown={};ctrlDown.type=INPUT_KEYBOARD;ctrlDown.ki.wVk=VK_LCONTROL;
                                 SendInput(1,&ctrlDown,sizeof(INPUT));
                                 pressKey('V');
                                 INPUT ctrlUp={};ctrlUp.type=INPUT_KEYBOARD;ctrlUp.ki.wVk=VK_LCONTROL;ctrlUp.ki.dwFlags=KEYEVENTF_KEYUP;
                                 SendInput(1,&ctrlUp,sizeof(INPUT));
                                 //根据之前获取到的状态，也就是说如果之前合成过键的抬起事件，那么合成对应键的按下事件
                                 if(wasLCtrlDown){ //左Ctrl键
                                     INPUT LCtrlDown={};LCtrlDown.type=INPUT_KEYBOARD;LCtrlDown.ki.wVk=VK_LCONTROL;
                                     SendInput(1,&LCtrlDown,sizeof(INPUT));
                                 }
                                 if(wasRCtrlDown){ //右Ctrl键
                                     INPUT RCtrlDown={};RCtrlDown.type=INPUT_KEYBOARD;RCtrlDown.ki.wVk=VK_RCONTROL;
                                     SendInput(1,&RCtrlDown,sizeof(INPUT));
                                 }
                                 if(wasLAltDown){ //左Alt键
                                     INPUT LAltDown={};LAltDown.type=INPUT_KEYBOARD;LAltDown.ki.wVk=VK_LMENU;
                                     SendInput(1,&LAltDown,sizeof(INPUT));
                                 }
                                 if(wasRAltDown){ //右Alt键
                                     INPUT RAltDown={};RAltDown.type=INPUT_KEYBOARD;RAltDown.ki.wVk=VK_RMENU;
                                     SendInput(1,&RAltDown,sizeof(INPUT));
                                 }
                                 if(wasLShiftDown){ //左Shift键
                                     INPUT LShiftDown={};LShiftDown.type=INPUT_KEYBOARD;LShiftDown.ki.wVk=VK_LSHIFT;
                                     SendInput(1,&LShiftDown,sizeof(INPUT));
                                 }
                                 if(wasRShiftDown){ //右Shift键
                                     INPUT RShiftDown={};RShiftDown.type=INPUT_KEYBOARD;RShiftDown.ki.wVk=VK_RSHIFT;
                                     SendInput(1,&RShiftDown,sizeof(INPUT));
                                 }
                                 if(wasLMetaDown){ //左Meta键
                                     INPUT LMetaDown={};LMetaDown.type=INPUT_KEYBOARD;LMetaDown.ki.wVk=VK_LWIN;
                                     SendInput(1,&LMetaDown,sizeof(INPUT));
                                 }
                                 if(wasRMetaDown){ //右Meta键
                                     INPUT RMetaDown={};RMetaDown.type=INPUT_KEYBOARD;RMetaDown.ki.wVk=VK_RWIN;
                                     SendInput(1,&RMetaDown,sizeof(INPUT));
                                 }
#endif
                             }
                            );
            itemHotkeys.append(hk);//把QHotkey *对象hk添加到动态数组中
        }
        else{ //如果hkStr为空字符串
            itemHotkeys.append(nullptr);//那就把nullptr添加到动态数组中
        }
    }
}

void xianshi(QWidget &chuang){ //如果窗口当前不可见，那么显示窗口
    if(!chuang.isVisible()) chuang.show();
#ifdef _WIN32
    SetForegroundWindow((HWND)chuang.winId());//窗口置顶
#endif
}

bool isValidHotkey(const QKeySequence & seq,QVector<QHotkey *> & itemHotkeys_,QKeySequenceEdit * edit_){ //检查快捷键是否合规，如果不合规那么弹出对应警告对话框。规则：1.包含至少一个修饰键（Ctrl/Alt/Shift/Meta）；2.有且只能有一个主键；3.快捷键不能已经存在动态数组itemHotkeys里，也就是说不能已被列表项使用；4.其他情况，比如用户输入了全局快捷键or已经被其他软件占用的快捷键，那么快捷键输入框会直接失去焦点，导致输入不了最后一个主键，也就是输入为空。这些情况就不用考虑了
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
    if(hasModifier&&hasPrimary){ //如果快捷键字符串满足要求
        for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有列表项对应的QHotkey *对象 //这里使用的是引用遍历
            if(hk){ //如果hk不为空指针
                if(hk->shortcut()==seq){ //返回hk对应的QKeySequence对象，如果它和seq完全相同，那么说明快捷键已被使用
                    QMessageBox::warning(edit_,"快捷键不合规","快捷键已被使用  ");
                    return false;
                }
            }
        }
        return true;//如果遍历完成后都没有return，那么返回true
    }
    else{
        QMessageBox::warning(edit_,"快捷键不合规","快捷键必须包含至少一个修饰键（Ctrl/Alt/Shift/Meta）和一个主键  ");
        return false;
    }
}

void adjustAllWindows(int w,int h, //根据宽高，设置所有窗口大小和所有控件大小，以及所有控件位置
                      QWidget & chuangkou,QListWidget & liebiao,QPushButton & shezhi,QPushButton & tianjia,QPushButton & tuding, //主窗口
                      QWidget & shezhichuangkou, //设置窗口
                      QWidget & tianjiachuangkou,QPlainTextEdit & tianjiakuang,QPushButton & tianjiaquxiao,QPushButton & tianjiaqueding, //添加窗口
                      QWidget & xiugaichuangkou,QPlainTextEdit & xiugaikuang,QPushButton & xiugaiquxiao,QPushButton & xiugaiqueding, //修改窗口
                      QWidget & kuaijiejianchuangkou,QLabel & kjjtishi,QKeySequenceEdit & kjjhotkeyEdit,QPushButton & kjjqingkong,QPushButton & kjjqueding //快捷键窗口
                     ){
    //主窗口
    chuangkou.setFixedSize(w,h);
    liebiao.move(0,46);
    liebiao.setFixedSize(w,h-40);//500*460 //把liebiao最下面的6个像素放到窗口外，隐藏起来。这样平行滚动条就不会不好看了
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
    tianjiakuang.setFixedSize(w,h-37);//500*463
    tianjiaquxiao.move(0,h-37);//0,463
    tianjiaquxiao.setFixedSize(w/2,33);//250*33
    tianjiaqueding.move(w/2,h-37);//250,463
    tianjiaqueding.setFixedSize(w/2,33);//250*33

    //修改窗口
    xiugaichuangkou.setFixedSize(w,h);
    xiugaikuang.move(0,0);
    xiugaikuang.setFixedSize(w,h-37);//500*463
    xiugaiquxiao.move(0,h-37);//0,463
    xiugaiquxiao.setFixedSize(w/2,33);//250*33
    xiugaiqueding.move(w/2,h-37);//250,463
    xiugaiqueding.setFixedSize(w/2,33);//250*33

    //快捷键窗口
    int kjjw=240;//固定快捷键窗口的宽高，不然不好看
    int kjjh=kjjw/3*2;//160 //还是3:2好看
    kuaijiejianchuangkou.setFixedSize(kjjw,kjjh);
    kjjtishi.move(10,0);
    kjjtishi.setFixedSize(kjjw-20,70);//220*70
    kjjhotkeyEdit.move(10,70);
    kjjhotkeyEdit.setFixedSize(kjjw-20,40);//220*40
    kjjqingkong.move(0,kjjh-36);//0,124
    kjjqingkong.setFixedSize(kjjw/2,33);//120*33
    kjjqueding.move(kjjw/2,kjjh-36);//120,124
    kjjqueding.setFixedSize(kjjw/2,33);//120*33
}

//自定义一个事件过滤器类
class MyEventFilter:public QObject{
private:
    QWidget * mainWin;//指向主窗口chuangkou
    QWidget * addWin;//指向添加窗口tianjiachuangkou
    QWidget * editWin;//指向修改窗口xiugaichuangkou
    QPushButton * addCancel;//指向添加窗口的取消按钮
    QPushButton * editCancel;//指向修改窗口的取消按钮
    QListWidget * liebiao;//指向主窗口里的列表
public:
    MyEventFilter(QWidget * m,QWidget * a,QWidget * e,QPushButton * ac,QPushButton * ec,QListWidget * l):mainWin(m),addWin(a),editWin(e),addCancel(ac),editCancel(ec),liebiao(l){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{
        if(event->type()==QEvent::KeyPress){ //如果是键盘按下事件
            QKeyEvent * keyEvent=static_cast<QKeyEvent*>(event);
            if(keyEvent->key()==Qt::Key_Escape){ //如果按下的是Esc键
                if(mainWin->isActiveWindow()){ //如果焦点在主窗口chuangkou
                    mainWin->close();//隐藏主窗口
                    return true;//拦截事件，不再向下传递
                }
                else if(addWin->isActiveWindow()){ //如果焦点在添加窗口
                    addCancel->click();//相当于按下“取消”按钮
                    return true;
                }
                else if(editWin->isActiveWindow()){ //如果焦点在修改窗口
                    editCancel->click();//相当于按下“取消”按钮
                    return true;
                }
            }
            if((keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter) && mainWin->isActiveWindow()){ //如果按下的是回车键，并且焦点在主窗口
                QListWidgetItem * item=liebiao->currentItem();//获取当前选中的列表项
                if(item){ //如果有选中的项，那么调用shuchu函数
                    shuchu(item,mainWin);
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj,event);//其他事件走默认处理
    }
};

//为快捷键输入框kjjhotkeyEdit单独自定义一个事件过滤器类，用于拦截kjjhotkeyEdit的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
class KjjHotkeyEditFilter:public QObject{
private:
    QKeySequenceEdit * edit_;//指向快捷键输入框kjjhotkeyEdit
    QVector<QHotkey *> & itemHotkeys_;//引用动态数组itemHotkeys
public:
    KjjHotkeyEditFilter(QKeySequenceEdit * e,QVector<QHotkey *> & itemHotkeys,QObject * parent=nullptr):edit_(e),itemHotkeys_(itemHotkeys),QObject(parent){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{ //重写eventFilter以拦截事件
        if(obj==edit_ && event->type()==QEvent::FocusIn){ //当kjjhotkeyEdit获得焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有列表项对应的QHotkey *对象 //这里使用的是引用遍历
                if(hk) hk->setRegistered(false);//如果hk不为空指针，那么禁用当前已注册的快捷键hk
            }
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点。此时用户可继续输入新快捷键
        }
        if(obj==edit_ && event->type()==QEvent::FocusOut){ //当kjjhotkeyEdit失去焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有列表项对应的QHotkey *对象
                if(hk) hk->setRegistered(true);//如果hk不为空指针，那么恢复当前已注册的快捷键hk
            }
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点
        }
        return QObject::eventFilter(obj,event);//其他事件走默认处理
    }
};

//为快捷键输入框hotkeyEdit单独自定义一个事件过滤器类，用于拦截hotkeyEdit的焦点事件，实现：1.当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复；2.当hotkeyEdit获得焦点时立即禁用当前已注册的全局快捷键；3.当hotkeyEdit失去焦点时判断用户输入的快捷键是否合规，合规的话就应用，不合规的话就恢复输入框为原始快捷键、弹出警告对话框
class HotkeyEditFilter:public QObject{
private:
    QKeySequenceEdit * edit_;//指向快捷键输入框hotkeyEdit
    QHotkey * hotkey_;//指向hotkey，就是那个QHotkey *对象
    QString configPath_;//指向config.json文件路径
    QVector<QHotkey *> & itemHotkeys_;//引用动态数组itemHotkeys
public:
    HotkeyEditFilter(QKeySequenceEdit * e,QHotkey * h,const QString & path,QVector<QHotkey *> & itemHotkeys,QObject * parent=nullptr):edit_(e),hotkey_(h),configPath_(path),itemHotkeys_(itemHotkeys),QObject(parent){}
protected:
    bool eventFilter(QObject * obj,QEvent * event) override{ //重写eventFilter以拦截事件
        if(obj==edit_ && event->type()==QEvent::FocusIn){ //当hotkeyEdit获得焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有列表项对应的QHotkey *对象
                if(hk) hk->setRegistered(false);//如果hk不为空指针，那么禁用当前已注册的快捷键hk
            }
            if(hotkey_) hotkey_->setRegistered(false);//如果hotkey_不为空指针，那么禁用当前已注册的全局快捷键
            return false;//返回false，不拦截事件，让快捷键输入框继续正常处理焦点。此时用户可继续输入新快捷键
        }
        if(obj==edit_ && event->type()==QEvent::FocusOut){ //当hotkeyEdit失去焦点
            for(auto & hk:itemHotkeys_){ //遍历动态数组，也就是遍历所有列表项对应的QHotkey *对象
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
                    saveConfig(configPath_);//调用saveConfig函数，写入程序设置到config.json
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

//为shezhichuangkou单独自定义一个继承自QWidget的子类，同时重写showEvent实现每次窗口show()的时候清除子控件的焦点，再把焦点交给窗口本身。这样窗口show()的时候就不会自动聚焦子控件了
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

QWidget * pchuangkou=nullptr;//全局指针，指向已经启动的主窗口，用于当用户启动程序时，如果已经有实例正在运行，那么显示正在运行的那个实例的主窗口

int main(int argc, char *argv[]){
    SingleApplication a(argc, argv);//将QApplication替换为SingleApplication。写了这句代码，就可以确保程序只有一个实例正在运行了。如果尝试启动第二个实例，那么会终止并通知第一个实例
    //当用户启动程序时，如果程序已经有实例正在运行，那么触发
    QObject::connect(&a,&SingleApplication::instanceStarted,
                     [](){
                         if(pchuangkou){
                             xianshi(*pchuangkou);
                             pchuangkou->activateWindow();//让chuangkou获得输入焦点
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
        background-color: #fcfcfc;                  /*列表背景：微灰白色，与列表项的#ffffff形成微妙对比，让界面更有深度感和专业感*/
        border: 1px solid #e5e5e5;                  /*列表边框：1像素浅灰色，营造阴影效果*/
        border-radius: 6px;                         /*列表圆角*/
        padding: 4px;                               /*列表内边距：上下左右4像素*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
        selection-background-color: transparent;    /*禁用默认选中样式*/
        alternate-background-color: transparent;    /*禁用交替行背景色*/
    }
    QListWidget::item{
        background-color: #ffffff;                  /*列表项背景：纯白色*/
        border: 1px solid #e5e5e5;                  /*列表项边框：1像素浅灰色*/
        border-radius: 4px;                         /*列表项圆角，与列表圆角一致*/
        padding: 12px 16px;                         /*列表项内边距：上下12像素，左右16像素*//*【【【注：想修改列表项高度在这里修改】】】*/
        margin: 2px 2px;                            /*列表项外边距：上下2像素，左右2像素*//*【【【注：想让列表项更紧凑一点在这里修改】】】*/
        color: #323130;                             /*列表项字体颜色：深灰色*/
        min-height: 40px;                           /*列表项最小高度*//*【【【注：想修改列表项最小高度在这里修改】】】*/
    }
    QListWidget::item:hover{
        background-color: #f9f9f9;                  /*鼠标悬停列表项背景：白色*/
        border-color: #d1d1d1;                      /*鼠标悬停列表项边框：浅灰色*/
    }
    QListWidget::item:selected{
        background-color: #e5f3ff;                  /*鼠标选中列表项背景：偏蓝一点的白色*/
        border-color: #0078d4;                      /*鼠标选中列表项边框：蓝色*/
        color: #005a9e;                             /*鼠标选中列表项字体颜色：深蓝色*/
    }
    QListWidget::item:selected:hover{
        background-color: #cce7ff;                  /*鼠标选中且悬停，列表项背景：浅蓝色*/
        border-color: #106ebe;                      /*鼠标选中且悬停，列表项边框：更深的蓝色*/
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
    /*====================数字输入框====================*/
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
    /*====================快捷键输入框====================*/
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
    /*====================右键菜单====================*/
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
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical{
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
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal{
        width: 0px;                                 /*隐藏滚动条自带的那个箭头*/
    }
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal{
        background-color: transparent;              /*滚动条轨道颜色：透明*/
    }
    /*====================悬停提示====================*/
    QToolTip{
        background-color: #ffffff;                  /*提示背景颜色：纯白色*/
        color: #222222;                             /*提示字体颜色：深灰色*/
        padding: 2px 1px;                           /*提示内边距：上下2像素，左右1像素*/
    }
    )");



    a.setQuitOnLastWindowClosed(false);//这里填false的话就是关闭窗口后让程序隐藏到托盘，继续在后台运行。此时如果不在托盘的右键菜单添加一个退出键，你就只能在任务管理器里关闭该程序了
    QString configPath=QCoreApplication::applicationDirPath()+"/config.json";//定义config.json文件路径 //QCoreApplication::applicationDirPath()返回的是可执行文件的目录路径（不包含文件名本身）
    loadConfig(configPath);//程序启动时调用loadConfig函数

    QWidget chuangkou;
    pchuangkou=&chuangkou;//创建主窗口时把地址赋值给全局指针，用于当用户启动程序时，如果已经有实例正在运行，那么显示正在运行的那个实例的主窗口
    chuangkou.setWindowTitle("QuickSay");
    chuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QListWidget liebiao(&chuangkou);
    liebiao.setFont(QFont("微软雅黑",10));
    QString dataPath=QCoreApplication::applicationDirPath()+"/data.json";//定义data.json文件路径
    loadListFromJson(liebiao,dataPath);//程序启动时调用loadListFromJson函数
    QVector<QHotkey *> itemHotkeys;//创建一个动态数组，保存所有列表项对应的QHotkey *对象，并且保证它们的下标（0~n-1）与列表项的行号一一对应。因此如果对应列表项的快捷键字符串为空字符串，那么对应QHotkey *对象为nullptr
    rebuildItemHotkeys(liebiao,itemHotkeys,&a);//程序启动时为liebiao中的列表项注册快捷键
    //当按下liebiao中的某个选项时，就调用shuchu函数
    QObject::connect(&liebiao,&QListWidget::itemClicked,
                     [&chuangkou](QListWidgetItem * item){
                         shuchu(item,&chuangkou);
                     }
                    );
    //实现liebiao选项拖动排序
    liebiao.setDragEnabled(true);//允许列表项被拖动
    liebiao.setAcceptDrops(true);//允许列表接收拖动放下的项
    liebiao.setDropIndicatorShown(true);//显示拖动放下时的指示器
    liebiao.setDefaultDropAction(Qt::MoveAction);//设置默认拖放行为为移动，而不是复制
    liebiao.setDragDropMode(QAbstractItemView::InternalMove);//设置内部移动模式，用户只能在列表内部拖动
    //监听模型的rowsMoved信号，当用户拖动完成后触发，写入列表内容到data.json
    QObject::connect(liebiao.model(),&QAbstractItemModel::rowsMoved,
                     [&](){
                         saveListToJson(liebiao,dataPath);
                         rebuildItemHotkeys(liebiao,itemHotkeys,&a);//拖动完成后为liebiao中的列表项注册快捷键
                     }
                    );

    //创建shezhichuangkou窗口
    bujihuoChuangkou shezhichuangkou;//使用我们自定义的那个子类bujihuoChuangkou创建
    shezhichuangkou.setWindowTitle("QuickSay-设置");
    shezhichuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QFormLayout * formLayout=new QFormLayout(&shezhichuangkou);//创建一个表单布局，并直接作用于设置窗口

    //全局快捷键设置
    QKeySequenceEdit hotkeyEdit( QKeySequence(config["hotkey"].toString()) ,&shezhichuangkou);//创建一个快捷键输入框，用于输入快捷键。里面一开始就存放着config里的快捷键
    formLayout->addRow("全局快捷键：",&hotkeyEdit);//在表单布局中添加一行，左边是标签“全局快捷键：”，右边是快捷键输入框hotkeyEdit
    QHotkey * hotkey=new QHotkey( QKeySequence(config["hotkey"].toString()) ,true,&a);//定义一个QHotkey *对象，设置快捷键为config里的快捷键，全局可用。此时就成功注册快捷键了，也就是说按下快捷键会发出信号 //这句代码里已经把a作为父对象传给了hotkey，a会自动管理其内存，不需要手动释放内存
    //设置按下全局快捷键后会怎样
    QObject::connect(hotkey,&QHotkey::activated,
                     [&chuangkou](){
                         xianshi(chuangkou);//显示chuangkou
                         chuangkou.activateWindow();//让chuangkou获得输入焦点
                     }
                    );
    //使用自定义的事件过滤器类HotkeyEditFilter，用于拦截hotkeyEdit的焦点事件，实现：1.当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复；2.当hotkeyEdit获得焦点时立即禁用当前已注册的全局快捷键；3.当hotkeyEdit失去焦点时判断用户输入的快捷键是否合规，合规的话就应用，不合规的话就恢复输入框为原始快捷键、弹出警告对话框
    HotkeyEditFilter * hkFilter=new HotkeyEditFilter(&hotkeyEdit,hotkey,configPath,itemHotkeys,&a);//创建事件过滤器对象
    hotkeyEdit.installEventFilter(hkFilter);//把事件过滤器安装到hotkeyEdit上

    //默认窗口大小设置
    QWidget * sizeWidget=new QWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout * sizeLayout=new QHBoxLayout(sizeWidget);//使用水平布局，目标效果是左边是标签“宽度”和宽度输入框，右边是标签“高度”和高度输入框
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
    formLayout->addRow("默认窗口大小：",sizeWidget);//在表单布局中添加一行，左边是标签“默认窗口大小：”，右边是“宽度”“高度”和两个输入框
    //如果用户在设置-默认窗口大小里修改了宽度/高度，那么写入程序设置到config.json，同时调整所有窗口大小，这个功能的实现代码我放最后面了

#ifdef _WIN32
    //开机自启动设置
    QWidget * autostartupWidget=new QWidget(&shezhichuangkou);//创建一个容器，用来包装水平布局
    QHBoxLayout * autostartupLayout=new QHBoxLayout(autostartupWidget);//使用水平布局，目标效果是里面有一个复选框。整这么麻烦是因为不这么做标签和复选框就上对齐，看起来不平行了
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
    QObject::connect(autostartupCheck,&QCheckBox::toggled,
                     [&autostartupRegPath,&autostartupRegName,&configPath](bool checked){
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
#endif

    //在chuangkou右上角放一个“设置”按钮
    QPushButton shezhi("",&chuangkou);//创建设置按钮，文本为空字符串。不然文本也会显示在按钮上
    shezhi.setObjectName("iconButton");//应用图标按钮样式
    shezhi.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/settings.svg"));//设置按钮图标
    shezhi.setIconSize(QSize(20,20));//调整图标大小为20*20像素
    shezhi.setToolTip("设置");//设置鼠标悬停提示文字为“设置”
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip,false);//禁用Qt自带的QToolTip悬停提示动画
    //当按下“设置”按钮时，弹出shezhichuangkou窗口
    QObject::connect(&shezhi,&QPushButton::clicked,
                     [&shezhichuangkou](){
                         xianshi(shezhichuangkou);
                     }
                    );

    //创建tianjiachuangkou窗口，里面有一个输入框，底下有一个“取消”按钮和一个“添加”按钮
    QWidget tianjiachuangkou;
    tianjiachuangkou.setWindowTitle("QuickSay-添加");
    tianjiachuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QPlainTextEdit tianjiakuang(&tianjiachuangkou);
    QPushButton tianjiaquxiao("取消",&tianjiachuangkou);
    QPushButton tianjiaqueding("添加",&tianjiachuangkou);
    //当按下“取消”按钮时，清空输入框并隐藏tianjiachuangkou窗口
    QObject::connect(&tianjiaquxiao,&QPushButton::clicked,
                     [&](){
                         tianjiakuang.clear();
                         tianjiachuangkou.close();
                     }
                    );
    //当按下“添加”按钮时，获取输入，把输入addItem到liebiao里，清空输入框，隐藏tianjiachuangkou窗口
    QObject::connect(&tianjiaqueding,&QPushButton::clicked,
                     [&](){
                         QString text=tianjiakuang.toPlainText();//获取输入框里的内容
                         if(!text.isEmpty()){ //如果获取到的内容不是空的
                             liebiao.addItem(text);//把输入内容添加到列表liebiao中
                             saveListToJson(liebiao,dataPath);//添加后写入列表内容到data.json
                             rebuildItemHotkeys(liebiao,itemHotkeys,&a);//添加后为liebiao中的列表项注册快捷键
                         }
                         tianjiakuang.clear();
                         tianjiachuangkou.close();
                     }
                    );

    //在chuangkou右上角放一个“添加”按钮
    QPushButton tianjia("",&chuangkou);//创建添加按钮，文本为空字符串
    tianjia.setObjectName("iconButton");//应用图标按钮样式
    tianjia.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/add.svg"));//设置按钮图标
    tianjia.setIconSize(QSize(20,20));//调整图标大小为20*20像素
    tianjia.setToolTip("添加语录");//设置鼠标悬停提示文字为“添加语录”
    //当按下“添加”按钮时，弹出tianjiachuangkou窗口
    QObject::connect(&tianjia,&QPushButton::clicked,
                     [&tianjiachuangkou,&tianjiakuang](){
                         tianjiakuang.clear();
                         xianshi(tianjiachuangkou);
                         tianjiachuangkou.activateWindow();//让tianjiachuangkou获得输入焦点
                         tianjiakuang.setFocus();//把焦点给到tianjiakuang，而不是其他控件
                     }
                    );

    //在chuangkou右上角放一个图钉按钮
    QPushButton tuding("",&chuangkou);//创建图钉按钮，文本为空字符串
    tuding.setObjectName("iconButton");//应用图标按钮样式
    if(config["tudingflag"].toBool()==false){ //如果没有钉住窗口
        tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/空心图钉.svg"));//设置按钮图标为空心图钉
        tuding.setToolTip("已关闭失去焦点不隐藏");//设置鼠标悬停提示文字为“已关闭失去焦点不隐藏”
    }
    else{ //如果钉住窗口
        tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/实心图钉.svg"));//设置按钮图标为实心图钉
        tuding.setToolTip("已开启失去焦点不隐藏");//设置鼠标悬停提示文字为“已开启失去焦点不隐藏”
    }
    tuding.setIconSize(QSize(20,20));//调整图标大小为20*20像素
    //当按下图钉按钮时，切换按钮图标
    QObject::connect(&tuding,&QPushButton::clicked,
                     [&tuding,&configPath](){
                         if(config["tudingflag"].toBool()==false){ //如果没有钉住窗口
                             tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/实心图钉.svg"));//切换按钮图标为实心图钉
                             tuding.setToolTip("已开启失去焦点不隐藏");//设置鼠标悬停提示文字为“已开启失去焦点不隐藏”
                             config["tudingflag"]=true;
                             saveConfig(configPath);//写入程序设置到config.json
                         }
                         else{ //如果钉住窗口
                             tuding.setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/空心图钉.svg"));//切换按钮图标为空心图钉
                             tuding.setToolTip("已关闭失去焦点不隐藏");//设置鼠标悬停提示文字为“已关闭失去焦点不隐藏”
                             config["tudingflag"]=false;
                             saveConfig(configPath);//写入程序设置到config.json
                         }
                     }
                    );
    //当程序失去焦点，并且只有主窗口可见、config["tudingflag"].toBool()==false时，隐藏主窗口，这个功能的实现代码我放最后面了

    //创建xiugaichuangkou窗口，里面有一个输入框，底下有一个“取消”按钮和一个“修改”按钮
    QListWidgetItem * currentEditingItem=nullptr;//记录用户点到的是liebiao中的哪个选项
    QWidget xiugaichuangkou;
    xiugaichuangkou.setWindowTitle("QuickSay-修改");
    xiugaichuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QPlainTextEdit xiugaikuang(&xiugaichuangkou);
    QPushButton xiugaiquxiao("取消",&xiugaichuangkou);
    QPushButton xiugaiqueding("修改",&xiugaichuangkou);
    //当按下“取消”按钮时，清空输入框并隐藏xiugaichuangkou窗口
    QObject::connect(&xiugaiquxiao,&QPushButton::clicked,
                     [&](){
                         xiugaikuang.clear();
                         xiugaichuangkou.close();
                     }
                    );
    //当按下“修改”按钮时，获取输入，把输入setText到被记录的选项里，清空输入框，隐藏xiugaichuangkou窗口
    QObject::connect(&xiugaiqueding,&QPushButton::clicked,
                     [&](){
                         QString text=xiugaikuang.toPlainText();//获取输入框里的内容
                         if(!text.isEmpty()){ //如果获取到的内容不是空的
                             currentEditingItem->setText(text);//把输入内容修改到列表liebiao中
                             saveListToJson(liebiao,dataPath);//修改后写入列表内容到data.json
                             rebuildItemHotkeys(liebiao,itemHotkeys,&a);//修改后为liebiao中的列表项注册快捷键
                         }
                         xiugaikuang.clear();
                         xiugaichuangkou.close();
                     }
                    );

    //创建kuaijiejianchuangkou窗口，里面有一个快捷键输入框，底下有一个“清空”按钮和一个“确定”按钮
    QListWidgetItem * kjjcurrentEditingItem=nullptr;//记录用户点到的是liebiao中的哪个选项
    QWidget kuaijiejianchuangkou;
    kuaijiejianchuangkou.setWindowTitle("设置快捷键");
    kuaijiejianchuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));
    QLabel kjjtishi("为当前语录设置快捷键，\n于是就能通过快捷键输入当前语录，\n而不必呼出界面选择：",&kuaijiejianchuangkou);
    QKeySequenceEdit kjjhotkeyEdit(&kuaijiejianchuangkou);//创建一个快捷键输入框，用于输入快捷键。里面一开始就存放着对应列表项的Qt::UserRole里的快捷键字符串，因为弹出窗口的时候就已经setKeySequence过了
    QPushButton kjjqingkong("清空",&kuaijiejianchuangkou);
    QPushButton kjjqueding("确定",&kuaijiejianchuangkou);
    //当按下“清空”按钮时
    QObject::connect(&kjjqingkong,&QPushButton::clicked,
                     [&](){
                         if(!kjjcurrentEditingItem){ //如果kjjcurrentEditingItem为nullptr //这个if就是以防万一用的，正常情况下不可能触发这个if
                             kuaijiejianchuangkou.close();//关闭kuaijiejianchuangkou
                             return ;//结束当前这个槽函数的执行
                         }
                         kjjcurrentEditingItem->setData(Qt::UserRole,QString(""));//把空字符串保存到对应列表项的Qt::UserRole里
                         saveListToJson(liebiao,dataPath);//清空后写入列表内容到data.json
                         rebuildItemHotkeys(liebiao,itemHotkeys,&a);//清空后为liebiao中的列表项注册快捷键
                         kuaijiejianchuangkou.close();
                     }
                    );
    //当按下“确定”按钮时
    QObject::connect(&kjjqueding,&QPushButton::clicked,
                     [&](){
                         if(!kjjcurrentEditingItem){ //如果kjjcurrentEditingItem为nullptr //这个if也是以防万一用的，正常情况下不可能触发这个if
                             kuaijiejianchuangkou.close();//关闭kuaijiejianchuangkou
                             return ;//结束当前这个槽函数的执行
                         }
                         QKeySequence seq=kjjhotkeyEdit.keySequence();//取出用户在输入框里输入的快捷键
                         if(!seq.isEmpty()){ //如果输入不为空
                             if(!isValidHotkey(seq,itemHotkeys,&kjjhotkeyEdit)){ //如果快捷键不合规（调用isValidHotkey函数检查快捷键是否合规）
                                 return ;//结束当前这个槽函数的执行，但是不关闭kuaijiejianchuangkou，于是用户可以重新在这个窗口输入快捷键
                             }
                             kjjcurrentEditingItem->setData(Qt::UserRole,seq.toString());//把用户输入的快捷键字符串保存到对应列表项的Qt::UserRole里
                         }
                         saveListToJson(liebiao,dataPath);//确定后写入列表内容到data.json
                         rebuildItemHotkeys(liebiao,itemHotkeys,&a);//确定后为liebiao中的列表项注册快捷键
                         kuaijiejianchuangkou.close();
                     }
                    );
    //使用自定义的事件过滤器类KjjHotkeyEditFilter，用于拦截kjjhotkeyEdit的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
    kjjhotkeyEdit.installEventFilter(   new KjjHotkeyEditFilter(&kjjhotkeyEdit,itemHotkeys,&a)   );//创建事件过滤器对象，并把它安装到kjjhotkeyEdit上

    //右键liebiao中的某个选项时，弹出一个菜单，上面有修改、删除、快捷键三个选项
    QMenu menu1;
    QAction xiugai("修改",&menu1);
    menu1.addAction(&xiugai);
    QAction shanchu("删除",&menu1);
    menu1.addAction(&shanchu);
    QAction kuaijiejian("快捷键",&menu1);
    menu1.addAction(&kuaijiejian);
    liebiao.setContextMenuPolicy(Qt::CustomContextMenu);
    //当右键liebiao时，执行lambda表达式
    QObject::connect(&liebiao,&QListWidget::customContextMenuRequested,
                     [&](const QPoint &pos){
                         QListWidgetItem * item=liebiao.itemAt(pos);//根据点击位置，找到对应的QListWidgetItem（如果点到空白区域则返回nullptr）
                         if(item){ //如果点到liebiao中的某个选项，那么弹出菜单，上面有修改、删除两个选项
                             QAction * selectedAction=menu1.exec(liebiao.mapToGlobal(pos));//在鼠标点击的位置弹出菜单，并等待用户选择一个QAction
                             if(selectedAction==&xiugai){ //如果用户选了“修改”
                                 //那么弹出xiugaichuangkou窗口，窗口里有选项中的文本，通过输入框可以修改语录
                                 currentEditingItem=item;//记录当前要修改的选项
                                 xiugaikuang.setPlainText(item->text());//把原来的文本放进输入框
                                 xianshi(xiugaichuangkou);
                                 xiugaichuangkou.activateWindow();//让xiugaichuangkou获得输入焦点
                                 xiugaikuang.setFocus();//把焦点给到xiugaikuang，而不是其他控件
                             }
                             else if(selectedAction==&shanchu){ //如果用户选了“删除”
                                 delete item;
                                 saveListToJson(liebiao,dataPath);//删除后写入列表内容到data.json
                                 rebuildItemHotkeys(liebiao,itemHotkeys,&a);//删除后为liebiao中的列表项注册快捷键
                             }
                             else if(selectedAction==&kuaijiejian){ //如果用户选了“快捷键”
                                 //那么弹出kuaijiejianchuangkou窗口，并且把快捷键字符串放进输入框
                                 kjjcurrentEditingItem=item;//记录当前要修改的选项
                                 kjjhotkeyEdit.setKeySequence( QKeySequence(item->data(Qt::UserRole).toString()) );//把这个列表项的Qt::UserRole里的快捷键字符串放进输入框
                                 xianshi(kuaijiejianchuangkou);
                                 kuaijiejianchuangkou.activateWindow();//让kuaijiejianchuangkou获得输入焦点
                                 kjjhotkeyEdit.setFocus();//把焦点给到kjjhotkeyEdit，而不是其他控件
                             }
                         }
                     }
                    );

    //创建托盘
    QSystemTrayIcon * trayIcon=new QSystemTrayIcon(&a);
    trayIcon->setIcon(QIcon(QCoreApplication::applicationDirPath()+"/icons/软件图标.svg"));//设置托盘图标
    //给托盘创建一个右键菜单
    QMenu * menu=new QMenu();
    QAction * shezhiAction=new QAction("设置",menu);//添加“设置”菜单项
    menu->addAction(shezhiAction);//shezhiAction的内存不用释放，因为此时menu会接管shezhiAction的所有权，自动管理其内存
    QObject::connect(shezhiAction,&QAction::triggered,
                     [&shezhichuangkou](){ //当按下“设置”菜单项时，弹出shezhichuangkou窗口
                         xianshi(shezhichuangkou);
                     }
                    );
    QAction * quitAction=new QAction("退出",menu);//添加“退出”菜单项
    menu->addAction(quitAction);//quitAction的内存也不用释放，因为此时menu会接管quitAction的所有权，自动管理其内存
    QObject::connect(quitAction,&QAction::triggered,&a,&QApplication::quit);
    trayIcon->setContextMenu(menu);//menu的内存也不用释放，因为此时trayIcon会接管menu的所有权，自动管理其内存
    trayIcon->show();
    trayIcon->setToolTip("QuickSay");//设置鼠标悬停在托盘图标上时显示的提示文字。这句代码必须写在show()之后

    //当程序失去焦点，并且只有主窗口可见、config["tudingflag"].toBool()==false时，隐藏主窗口
    QObject::connect(&a,&QApplication::applicationStateChanged,
                     [&](Qt::ApplicationState state){
                         if(( state==Qt::ApplicationInactive&&chuangkou.isVisible() )&&config["tudingflag"].toBool()==false){ //如果程序失去焦点，并且主窗口可见、config["tudingflag"].toBool()==false
                             if(( !shezhichuangkou.isVisible() )&&( !tianjiachuangkou.isVisible() )&&( !xiugaichuangkou.isVisible() )) //如果设置窗口、添加窗口、修改窗口都不可见
                                 chuangkou.close();
                         }
                     }
                    );

    //使用自定义的事件过滤器类MyEventFilter
    MyEventFilter * filter=new MyEventFilter(&chuangkou,&tianjiachuangkou,&xiugaichuangkou,&tianjiaquxiao,&xiugaiquxiao,&liebiao);//创建事件过滤器对象
    a.installEventFilter(filter);//安装事件过滤器 //安装到应用级别（就是那个a），这样能捕捉所有窗口的键盘事件

    //从全局对象config读取默认窗口大小，然后根据宽高，设置所有窗口大小和所有控件大小，以及所有控件位置
    adjustAllWindows(config["width"].toInt(),config["height"].toInt(),
                     chuangkou,liebiao,shezhi,tianjia,tuding,
                     shezhichuangkou,
                     tianjiachuangkou,tianjiakuang,tianjiaquxiao,tianjiaqueding,
                     xiugaichuangkou,xiugaikuang,xiugaiquxiao,xiugaiqueding,
                     kuaijiejianchuangkou,kjjtishi,kjjhotkeyEdit,kjjqingkong,kjjqueding
                    );
    //如果用户在设置-默认窗口大小里修改了宽度，那么写入程序设置到config.json，同时调整所有窗口大小和所有控件大小，以及所有控件位置
    QObject::connect(widthSpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int w){
                         config["width"]=w;//更新config里的宽度
                         config["height"]=heightSpin->value();//读取当前高度，然后更新config里的高度
                         saveConfig(configPath);//写入程序设置到config.json
                         //调用adjustAllWindows函数，调整所有窗口大小
                         adjustAllWindows(config["width"].toInt(),config["height"].toInt(),
                                          chuangkou,liebiao,shezhi,tianjia,tuding,
                                          shezhichuangkou,
                                          tianjiachuangkou,tianjiakuang,tianjiaquxiao,tianjiaqueding,
                                          xiugaichuangkou,xiugaikuang,xiugaiquxiao,xiugaiqueding,
                                          kuaijiejianchuangkou,kjjtishi,kjjhotkeyEdit,kjjqingkong,kjjqueding
                                         );
                     }
                    );
    //如果用户在设置-默认窗口大小里修改了高度，那么写入程序设置到config.json，同时调整所有窗口大小和所有控件大小，以及所有控件位置
    QObject::connect(heightSpin,QOverload<int>::of(&QSpinBox::valueChanged),
                     [&](int h){
                         config["width"]=widthSpin->value();//读取当前宽度，然后更新config里的宽度
                         config["height"]=h;//更新config里的高度
                         saveConfig(configPath);//写入程序设置到config.json
                         //调用adjustAllWindows函数，调整所有窗口大小
                         adjustAllWindows(config["width"].toInt(),config["height"].toInt(),
                                          chuangkou,liebiao,shezhi,tianjia,tuding,
                                          shezhichuangkou,
                                          tianjiachuangkou,tianjiakuang,tianjiaquxiao,tianjiaqueding,
                                          xiugaichuangkou,xiugaikuang,xiugaiquxiao,xiugaiqueding,
                                          kuaijiejianchuangkou,kjjtishi,kjjhotkeyEdit,kjjqingkong,kjjqueding
                                         );
                     }
                    );

#ifdef _WIN32
    if(!   a.arguments().contains("--autostart")   ){ //程序启动时检查程序启动参数，如果没有包含我们专门为开机自启添加的标记“--autostart”（也就是说用户是通过双击可执行文件打开的程序，而不是通过开机自启自动打开的程序）
        xianshi(chuangkou);
        chuangkou.activateWindow();//让chuangkou获得输入焦点
    }
    //否则就是通过开机自启自动打开的程序，那么什么也不做，就后台运行个托盘
#endif



    return a.exec();
}
