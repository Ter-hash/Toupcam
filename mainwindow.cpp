#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_hcam(nullptr), m_count(0)
    , m_timer(new QTimer(this))
    , m_imgWidth(0), m_imgHeight(0), m_pData(nullptr)
    , m_res(0), m_temp(TOUPCAM_TEMP_DEF), m_tint(TOUPCAM_TINT_DEF)
    , form(new Form)
{
    ui->setupUi(this);
    //控件赋值
    m_cmb_res=ui->m_cmb_res;
    m_lbl_video=ui->m_lbl_video;
    m_lbl_display_name = ui->m_lbl_display_name;
    //打开按钮
    m_btn_open=ui->m_btn_open;
    //截图按钮
    m_btn_snap = ui->m_btn_snap;
    m_btn_snap->setEnabled(false);
    //暂停按钮
    m_btn_pause=ui->m_btn_pause;
    m_btn_pause->setEnabled(false);
    //垂直翻转按钮
    m_btn_vflip=ui->m_btn_vflip;
    m_btn_vflip->setEnabled(false);
    //水平翻转按钮
    m_btn_hflip=ui->m_btn_hflip;
    m_btn_hflip->setEnabled(false);
    //曝光设置
    m_slider_expoTime = ui->m_slider_expoTime;
    m_slider_expoTime->setEnabled(false);
    m_slider_expoGain = ui->m_slider_expoGain;
    m_slider_expoGain->setEnabled(false);
    m_lbl_expoTime = ui->m_lbl_expoTime;
    m_lbl_expoGain = ui->m_lbl_expoGain;
    m_cbox_auto = ui->m_cbox_auto;
    m_cbox_auto->setEnabled(false);
    //白平衡设置
    m_slider_temp=ui->m_slider_temp;

    m_slider_temp->setRange(TOUPCAM_TEMP_MIN, TOUPCAM_TEMP_MAX);
    m_slider_temp->setValue(TOUPCAM_TEMP_DEF);
    m_slider_tint=ui->m_slider_tint;
    m_slider_tint->setRange(TOUPCAM_TINT_MIN, TOUPCAM_TINT_MAX);
    m_slider_tint->setValue(TOUPCAM_TINT_DEF);
    m_slider_temp->setEnabled(false);
    m_slider_tint->setEnabled(false);
    m_lbl_temp=ui->m_lbl_temp;
    m_lbl_tint=ui->m_lbl_tint;

    m_cbox_autoWB = ui->m_cbox_autoWB;
    m_cbox_autoWB->setEnabled(false);


    //    m_btn_autoWB =ui->m_btn_autoWB;
    //    m_btn_autoWB->setEnabled(false);
    //菜单命令绑定槽函数
    connect(ui->action_open, SIGNAL(triggered()), this, SLOT(on_action_open_clicked()));
    connect(ui->action_close_camera, SIGNAL(triggered()), this, SLOT(on_action_close_clicked()));

    connect(m_cmb_res, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        if (m_hcam) //step 1: stop camera
            Toupcam_Stop(m_hcam);

        m_res = index;
        m_imgWidth = m_cur.model->res[index].width;
        m_imgHeight = m_cur.model->res[index].height;

        if (m_hcam) //step 2: restart camera
        {
            Toupcam_put_eSize(m_hcam, static_cast<unsigned>(m_res));
            startCamera();
        }
    });

    //状态栏信息显示
    connect(m_timer, &QTimer::timeout, this, [this]()
    {
        unsigned nFrame = 0, nTime = 0, nTotalFrame = 0;

        if (m_hcam && SUCCEEDED(Toupcam_get_FrameRate(m_hcam, &nFrame, &nTime, &nTotalFrame)) && (nTime > 0))
        {

            ui->statusbar->showMessage(QString::asprintf("%u, fps = %.1f", nTotalFrame, nFrame * 1000.0 / nTime));
        }
    });
    showCamera();
    //事件响应
    connect(this, &MainWindow::evtCallback, this, [this](unsigned nEvent)
    {
        //全局事件
        //运行时事件
        /* this run in the UI thread */
        if (m_hcam)
        {
            if (TOUPCAM_EVENT_IMAGE == nEvent)
                //显示图像
                handleImageEvent();
            else if (TOUPCAM_EVENT_EXPOSURE == nEvent)
                handleExpoEvent();
            else if (TOUPCAM_EVENT_TEMPTINT == nEvent)
                handleTempTintEvent();
            else if (TOUPCAM_EVENT_STILLIMAGE == nEvent)
                handleStillImageEvent();
            else if (TOUPCAM_EVENT_ERROR == nEvent)
            {
                closeCamera();
                QMessageBox::warning(this, "Warning", "Generic error.");
            }
            else if (TOUPCAM_EVENT_DISCONNECTED == nEvent)
            {
                closeCamera();
                QMessageBox::warning(this, "Warning", "Camera disconnect.");
            }
        }
    });
    //自动曝光槽函数
    connect(m_cbox_auto, &QCheckBox::stateChanged, this, [this](bool state)
    {
        if (m_hcam)
        {
            Toupcam_put_AutoExpoEnable(m_hcam, state ? 1 : 0);
            m_slider_expoTime->setEnabled(!state);
            m_slider_expoGain->setEnabled(!state);
        }
    });

    connect(m_slider_expoTime, &QSlider::valueChanged, this, [this](int value)
    {
        if (m_hcam)
        {
            m_lbl_expoTime->setText(QString::number(value));
            if (!m_cbox_auto->isChecked())
                Toupcam_put_ExpoTime(m_hcam, value);
        }
    });

    connect(m_slider_expoGain, &QSlider::valueChanged, this, [this](int value)
    {
        if (m_hcam)
        {
            m_lbl_expoGain->setText(QString::number(value));
            if (!m_cbox_auto->isChecked())
                Toupcam_put_ExpoAGain(m_hcam, value);
        }
    });
    connect(m_slider_temp, &QSlider::valueChanged, this, [this](int value)
    {
        m_temp = value;
        if (m_hcam)
            Toupcam_put_TempTint(m_hcam, m_temp, m_tint);
        m_lbl_temp->setText(QString::number(value));
    });

    connect(m_slider_tint, &QSlider::valueChanged, this, [this](int value)
    {
        m_tint = value;
        if (m_hcam)
            Toupcam_put_TempTint(m_hcam, m_temp, m_tint);
        m_lbl_tint->setText(QString::number(value));
    });
}


void MainWindow::mousePressEvent(QMouseEvent *event)
{
    qDebug() << QString("%1...").arg(m_bar_value);
    if(m_bar_value>=ui->progressBar->maximum())
    {
        m_bar_value=ui->progressBar->minimum();
    }
    ui->progressBar->setValue(m_bar_value);
    m_bar_value += 5;
}
MainWindow::~MainWindow()
{
    delete ui;
}

//菜单栏打开命令
void MainWindow::on_action_open_clicked()
{
    QString filename;
    filename=QFileDialog::getOpenFileName(this,tr("选择图像"),"",tr("Images (*.png *.bmp *.jpg *.tif *.GIF )"));
    if(filename.isEmpty()){
        return;
    }else{
        QImage* img=new QImage;
        if(!( img->load(filename) ) ) //加载图像
        {
            QMessageBox::information(this,tr("打开图像失败"),tr("打开图像失败!"));
            delete img;
            return;
        }
        //        ui->label->setPixmap(QPixmap::fromImage(*img));
        //缩放显示图像
        QImage* imgScaled = new QImage;
        *imgScaled=img->scaled(400, 300, Qt::KeepAspectRatio);
        m_lbl_video->setPixmap(QPixmap::fromImage(*imgScaled));
    }
}

//菜单栏关闭命令
void MainWindow::on_action_close_clicked()
{
    m_lbl_video->clear();
    m_lbl_video->clearMask();
}


//关闭相机
void MainWindow::closeCamera()
{

    if (m_hcam)
    {
        Toupcam_Close(m_hcam);
        m_hcam = nullptr;
    }
    delete[] m_pData;
    m_pData = nullptr;

    m_btn_open->setText("打开相机");
    m_btn_pause->setText("暂停");
    m_btn_pause->setEnabled(false);
    m_btn_snap->setEnabled(false);
    m_btn_hflip->setEnabled(false);
    m_btn_vflip->setEnabled(false);

    m_cmb_res->setEnabled(false);
    m_cmb_res->clear();
    m_lbl_video->clear();
    m_timer->stop();
    ui->statusbar->clearMessage();
    //    m_lbl_frame->clear();
    m_cbox_auto->setEnabled(false);
    m_slider_expoGain->setEnabled(false);
    m_slider_expoTime->setEnabled(false);
    //    m_btn_autoWB->setEnabled(false);
    m_cbox_autoWB->setEnabled(false);
    m_slider_temp->setEnabled(false);
    m_slider_tint->setEnabled(false);
}

void MainWindow::startCamera()
{
    //启动相机
    if (m_pData)
    {
        delete[] m_pData;
        m_pData = nullptr;
    }
    m_pData = new uchar[TDIBWIDTHBYTES(m_imgWidth * 24) * m_imgHeight];
    unsigned uimax = 0, uimin = 0, uidef = 0;
    unsigned short usmax = 0, usmin = 0, usdef = 0;
    Toupcam_get_ExpTimeRange(m_hcam, &uimin, &uimax, &uidef);
    m_slider_expoTime->setRange(uimin, uimax);
    Toupcam_get_ExpoAGainRange(m_hcam, &usmin, &usmax, &usdef);
    m_slider_expoGain->setRange(usmin, usmax);
    if (0 == (m_cur.model->flag & TOUPCAM_FLAG_MONO))
        handleTempTintEvent();
    handleExpoEvent();
    if (SUCCEEDED(Toupcam_StartPullModeWithCallback(m_hcam, eventCallBack, this)))
    {
        m_cmb_res->setEnabled(true);
        m_btn_open->setText("关闭相机");
        m_btn_pause->setEnabled(true);
        m_btn_snap->setEnabled(true);
        m_btn_hflip->setEnabled(true);
        m_btn_vflip->setEnabled(true);

        char sn[32] = {0};
        char fwver[16] ={0};
        char hwver[16]={0};
        Toupcam_get_SerialNumber(m_hcam, sn);
        Toupcam_get_FwVersion(m_hcam, fwver);
        Toupcam_get_HwVersion(m_hcam, hwver);
        ui->m_lbl_fwver->setText(QString::asprintf("FwVersion:%s",fwver));
        ui->m_lbl_hwver->setText(QString::asprintf("HwVersion:%s",hwver));
        ui->m_lbl_sn->setText(QString::asprintf("SerialNumber:%s",sn));
        m_cbox_auto->setEnabled(true);
        //                m_btn_autoWB->setEnabled(true);
        m_cbox_autoWB->setEnabled(true);
        m_slider_temp->setEnabled(0 == (m_cur.model->flag & TOUPCAM_FLAG_MONO));
        m_slider_tint->setEnabled(0 == (m_cur.model->flag & TOUPCAM_FLAG_MONO));
        int bAuto = 0;
        Toupcam_get_AutoExpoEnable(m_hcam, &bAuto);
        m_cbox_auto->setChecked(1 == bAuto);

        m_timer->start(1000);
    }
    else
    {
        closeCamera();
        QMessageBox::warning(this, "Warning", "Failed to start camera.");
    }
}

//打开相机
void MainWindow::openCamera()
{

    m_hcam = Toupcam_Open(m_cur.id);
    if (m_hcam)
    {
        Toupcam_get_eSize(m_hcam, (unsigned*)&m_res);
        m_imgWidth = m_cur.model->res[m_res].width;
        m_imgHeight = m_cur.model->res[m_res].height;
        {
            const QSignalBlocker blocker(m_cmb_res);
            m_cmb_res->clear();
            for (unsigned i = 0; i < m_cur.model->preview; ++i)
            {
                m_cmb_res->addItem(QString::asprintf("%u*%u", m_cur.model->res[i].width, m_cur.model->res[i].height));
            }
            m_cmb_res->setCurrentIndex(m_res);
            m_cmb_res->setEnabled(true);
        }

        Toupcam_put_Option(m_hcam, TOUPCAM_OPTION_BYTEORDER, 0); //Qimage use RGB byte order
        Toupcam_put_AutoExpoEnable(m_hcam, 1);
        startCamera();
        QMessageBox::information(this, "Success", QString::asprintf("Camera %ls Connected.", m_cur.displayname), QMessageBox::Ok);
    }
}

//选择相机
//需要监听事件
void MainWindow::showCamera()
{
    // 显示可用相机
    ToupcamDeviceV2 arr[TOUPCAM_MAX] = { 0 };
    unsigned count = Toupcam_EnumV2(arr);
    if (0 == count){
        QMessageBox::warning(this, "Warning", "No Camera Found.");
        m_lbl_display_name->setText("No Camera.");
    }
    else
    {
        for (unsigned i = 0; i < count; ++i)
        {
            //循环每个相机
            m_cur = arr[i];
            m_lbl_display_name->setText(QString::fromWCharArray(m_cur.displayname));
        }
    }
}


//打开相机按钮槽函数
void MainWindow::on_m_btn_open_clicked()
{
    if (m_hcam)
        //关闭连接
        closeCamera();
    else
    {
        //打开相机
        openCamera();
    }
}


void MainWindow::on_m_btn_snap_clicked()
{
    if (m_hcam)
    {
        if (0 == m_cur.model->still)    // not support still image capture
        {
            if (m_pData)
            {
                QImage image(m_pData, m_imgWidth, m_imgHeight, QImage::Format_RGB888);
                image.save(QString::asprintf("./toupcam_%u.jpg", ++m_count));
            }
        }
        else
        {
            Toupcam_Snap(m_hcam, m_res);
            //            QMenu menu;
            //            for (unsigned i = 0; i < m_cur.model->still; ++i)
            //            {
            //                menu.addAction(QString::asprintf("%u*%u", m_cur.model->res[i].width, m_cur.model->res[i].height), this, [this, i](bool)
            //                {
            //                    Toupcam_Snap(m_hcam, i);
            //                    QMessageBox::information(this, "Success", QString::asprintf("Snap Success."));
            //                });
            //            }
            //            menu.exec(mapToGlobal(m_btn_snap->pos()));
        }
    }
}



//事件调用
void MainWindow::eventCallBack(unsigned nEvent, void* pCallbackCtx)
{
    MainWindow* pThis = reinterpret_cast<MainWindow*>(pCallbackCtx);
    emit pThis->evtCallback(nEvent);
}


//显示图像事件
void MainWindow::handleImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Toupcam_PullImage(m_hcam, m_pData, 24, &width, &height)))
    {
        QImage image(m_pData, width, height, QImage::Format_RGB888);
        QImage newimage = image.scaled(m_lbl_video->width(), m_lbl_video->height(), Qt::KeepAspectRatio, Qt::FastTransformation);
        m_lbl_video->setPixmap(QPixmap::fromImage(newimage));
    }
}


void MainWindow::handleExpoEvent()
{
    unsigned time = 0;
    unsigned short gain = 0;
    Toupcam_get_ExpoTime(m_hcam, &time);
    Toupcam_get_ExpoAGain(m_hcam, &gain);
    {
        const QSignalBlocker blocker(m_slider_expoTime);
        m_slider_expoTime->setValue(int(time));
    }
    {
        const QSignalBlocker blocker(m_slider_expoGain);
        m_slider_expoGain->setValue(int(gain));
    }
    m_lbl_expoTime->setText(QString::number(time));
    m_lbl_expoGain->setText(QString::number(gain));
}


void MainWindow::handleTempTintEvent()
{
    int nTemp = 0, nTint = 0;
    if (SUCCEEDED(Toupcam_get_TempTint(m_hcam, &nTemp, &nTint)))
    {
        {
            const QSignalBlocker blocker(m_slider_temp);
            m_slider_temp->setValue(nTemp);
        }
        {
            const QSignalBlocker blocker(m_slider_tint);
            m_slider_tint->setValue(nTint);
        }
        m_lbl_temp->setText(QString::number(nTemp));
        m_lbl_tint->setText(QString::number(nTint));
    }
}


//保存图片事件TOUPCAM_EVENT_STILLIMAGE
void MainWindow::handleStillImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Toupcam_PullStillImage(m_hcam, nullptr, 24, &width, &height))) // peek
    {
        std::vector<uchar> vec(TDIBWIDTHBYTES(width * 24) * height);
        if (SUCCEEDED(Toupcam_PullStillImage(m_hcam, &vec[0], 24, &width, &height)))
        {
            QImage image(&vec[0], width, height, QImage::Format_RGB888);
            image.save(QString::asprintf("toupcam_%u.jpg", ++m_count));
        }
    }
}


//暂停相机
void MainWindow::on_m_btn_pause_toggled(bool checked)
{
    Toupcam_Pause(m_hcam, checked);
    if(checked){
        m_btn_pause->setText("继续");
    }
    else{
        m_btn_pause->setText("暂停");
    }

}


//void MainWindow::on_m_btn_hflip_clicked()
//{

//}

//水平翻转
void MainWindow::on_m_btn_hflip_toggled(bool checked)
{
    if(checked){
        Toupcam_put_HFlip(m_hcam,checked);
    }
    else{
        Toupcam_put_HFlip(m_hcam,checked);
    }

}

//垂直翻转
void MainWindow::on_m_btn_vflip_toggled(bool checked)
{
    if(checked){
        Toupcam_put_VFlip(m_hcam,checked);
    }
    else{
        Toupcam_put_VFlip(m_hcam,checked);
    }
}




void MainWindow::on_actionAbout_triggered()
{

    form->show();
}

//打开子菜单
void MainWindow::on_pushButton_clicked()
{
    form->show();
}


void MainWindow::on_m_cmb_res_currentIndexChanged(int index)
{
    m_res=index;
}


void MainWindow::on_pushButton_2_clicked()
{
    Toupcam_put_Roi(m_hcam, 20, 20, 500, 500);
//    ui->label_12->setText();

}


void MainWindow::on_pushButton_3_clicked()
{
    Toupcam_put_Roi(m_hcam, 0, 0, 0, 0);
}

