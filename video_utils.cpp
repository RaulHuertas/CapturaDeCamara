#include "video_utils.hpp"
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QDir>

using namespace cv;


static std::unique_ptr<cv::VideoWriter> webcamMat;

qulonglong startTimeStamp = 0;

QString getStorageFolder(){
    auto mediaRoot = QString("/media/")+QString(qgetenv("USER"));
    QDir directory(mediaRoot);
    QStringList carpetas = directory.entryList(QStringList(),QDir::Dirs|QDir::NoDotAndDotDot);
    if(carpetas.size()>0){
        auto newbase = mediaRoot+"/"+carpetas[0]+"/videosCamaras";
        if(!QDir(newbase).exists()){
            QDir().mkdir(newbase);
        }
        return newbase;
    }

    return ".";
}

void restartStream( int c_w, int c_h, int c_fps, qulonglong timestamp){

    auto tm_str = QString::number(timestamp);
    auto strgFolder = getStorageFolder();
    QString webcam_name_str = strgFolder+"/"+QString("webcam_")+tm_str+QString(".avi");
    webcamMat = std::make_unique<VideoWriter>();
    webcamMat->open(webcam_name_str.toStdString(), VideoWriter::fourcc('X','2','6','4'), c_fps, cv::Size(c_w,c_h), true);
}

void video_checkTime( int c_w, int c_h, int c_fps, qulonglong timestamp){
    //video de maximo dos minutos
    if((timestamp-startTimeStamp)>2*60*1000){
        restartStream(c_w, c_h, c_fps, timestamp);
        startTimeStamp = timestamp;
    }
}

void video_start(
     int c_w, int c_h, int c_fps
){
    //v_cap = std::make_unique<cv::VideoCapture>(-1);
    auto timestamp = QDateTime::currentMSecsSinceEpoch();
    video_checkTime(c_w, c_h, c_fps, timestamp);
}

void video_close(){
    webcamMat = nullptr;
}

void video_addFrame_webcam(cv::Mat& frame){
    webcamMat->write(frame);
}

