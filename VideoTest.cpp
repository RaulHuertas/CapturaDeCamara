
#include "opencv2/opencv.hpp"
#include <sys/time.h>
#include <iostream>
#include <vector>
// api udp
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <thread>
#include <signal.h>
#include <QDateTime>
#include <unistd.h>
#include "video_utils.hpp"
#include  <atomic>
#include <fcntl.h>

using namespace std;
using namespace cv;

void errorPrograma(char *msg) {
    perror(msg);
    exit(0);
}

unsigned long long tiempoActual_ms(){
    timeval time;
    gettimeofday(&time, NULL);
    unsigned long long  millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);
    return millis;
}


void transmitirPorUDP(
                      int ancho, int alto, int formatoStream,
                      const std::vector<uchar>& imagenComprimida,
                      int sockfd, struct sockaddr_in& serveraddr, int serverlen
                      ){
    
    constexpr int sizePaquetes = 2*2048;
    constexpr int headerSize = 1+5*sizeof(int);
    int nPacks = imagenComprimida.size()/sizePaquetes;
    int bytesSobrantes = imagenComprimida.size()%sizePaquetes;
    //int totalSize = nPacks*(1+4*sizeof(int)+sizePaquetes);/
    if(bytesSobrantes>0){
        nPacks++;
    }
    int offsetDatos =0;
    std::vector<uchar> bufferTrans;
    bufferTrans.resize(headerSize+sizePaquetes);
    int porEnviar = imagenComprimida.size();
    const int totalImageSize=imagenComprimida.size();
    for(int p = 0; p<nPacks; p++){
        uchar *datos = bufferTrans.data();
        datos[0] = 0xFF; datos+=1;
        //cout<<"EnviandoOFFSET: "<<offsetDatos<<endl;
        memcpy(datos, &offsetDatos, sizeof(offsetDatos)); datos+=sizeof(offsetDatos);
        memcpy(datos, &totalImageSize, sizeof(totalImageSize)); datos+=sizeof(totalImageSize);
        memcpy(datos, &ancho, sizeof(ancho)); datos+=sizeof(ancho);
        memcpy(datos, &alto, sizeof(alto)); datos+=sizeof(alto);
        memcpy(datos, &formatoStream, sizeof(formatoStream)); datos+=sizeof(formatoStream);
        int nDatosAEnviarEnEstePaquete = std::min(sizePaquetes, porEnviar);
        memcpy(
               datos,
               &imagenComprimida[offsetDatos],
               nDatosAEnviarEnEstePaquete
               );datos+=nDatosAEnviarEnEstePaquete;
        const int packSize=nDatosAEnviarEnEstePaquete+headerSize;
        sendto(
               sockfd,
               bufferTrans.data(),
               packSize,
               0,
               (sockaddr*)&serveraddr,
               serverlen
               );
        
        porEnviar-=nDatosAEnviarEnEstePaquete;
        offsetDatos+=nDatosAEnviarEnEstePaquete;
    }
    
}

volatile sig_atomic_t terminar = 0;

void signal_handler(int signal)
{
    terminar = 1;
}

std::atomic_bool transmitir;
std::atomic_bool grabar;

int main( int argc, char** argv )
{
    char* hostname;
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent* server;
    int serverlen;
    int fpsImagen = 0;
    transmitir = true;
    grabar = true;
    if(argc!=5){
        printf("Argumentos del programa invalidos\n");
        printf("Invocarlo como: Test2 <direccionDestino>  <puertoDestino> <ancho> <alto>\n");
        exit(EXIT_FAILURE);
    }
    //Capturar señal de Ctrl+C para terminar e lprograma
    signal(SIGINT, signal_handler);
    
    //Captura de parametros de la linea de comandos
    int anchoImagen, altoImagen;
    hostname = argv[1];
    portno = atoi(argv[2]);
    anchoImagen = atoi(argv[3]);
    altoImagen = atoi(argv[4]);

    //CREANDO SOCKET UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        errorPrograma("ERROR abriendo el socket");
    }
    server = gethostbyname(hostname);
    if(server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    serverlen = sizeof(serveraddr);
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    /// Load the source image
    VideoCapture cap(detectUVCCamera());
    cap.set(CV_CAP_PROP_FRAME_WIDTH, anchoImagen);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, altoImagen);
    if(!cap.isOpened()){
        cout<<"No se puede abrir el dispositivo de captura "<<endl;
        return -1;
    }
    fpsImagen = cap.get(CV_CAP_PROP_FPS);
    cout<<"CAP_PROP_FRAME_WIDTH: "<<cap.get(CV_CAP_PROP_FRAME_WIDTH)<<endl;
    cout<<"CAP_PROP_FRAME_HEIGHT: "<<cap.get(CV_CAP_PROP_FRAME_HEIGHT)<<endl;
    cout<<"CAP_PROP_FPS: "<<fpsImagen<<endl;
    //cout<<"CAP_PROP_FOURCC: "<<cap.get(CV_CAP_PROP_FOURCC)<<endl;
    int formatoStream = cap.get(CV_CAP_PROP_FORMAT);
    //cout<<"CAP_PROP_FORMAT: "<<formatoStream<<endl;
    //cout<<"CAP_PROP_FORMAT ESQ: "<<((formatoStream==CV_8UC(3))?"true":"false")<<endl;
    
    
    unsigned long long marcaTiempoInicial = tiempoActual_ms();
    int cuadrosProcesados = 0;
    std::vector<uchar> imagenComprimida;
    imagenComprimida.reserve(1024*768);
    std::vector<int> parametrosCompresion(2);
    parametrosCompresion[0] = cv::IMWRITE_JPEG_QUALITY;
    parametrosCompresion[1] = 95;
    Mat* frame = nullptr;


    video_start(anchoImagen, altoImagen, fpsImagen);

    unsigned char rcvBuffer[4];

    int tx_webcam_dly=0;
    int tx_webcam_dly_ctr=0;
    qulonglong tx_webcam_lastRex=0;
    qulonglong thisIterationTime = QDateTime::currentMSecsSinceEpoch();


    while(terminar==0)
    {
        //iniciar el procesamiento de la imagen ya capturada
        std::thread hiloProc([&](){

            if(frame!=nullptr){
                thisIterationTime = QDateTime::currentMSecsSinceEpoch();
                cv::imencode(
                             ".jpg", *frame, imagenComprimida,
                             parametrosCompresion
                             );
                auto& pict = *frame;
                if(transmitir && (tx_webcam_dly_ctr==0)){
                    transmitirPorUDP(pict.cols, pict.rows, formatoStream,
                                 imagenComprimida, sockfd, serveraddr, serverlen);
                }
                if(grabar){
                    video_addFrame_webcam(*frame);
                }
                cuadrosProcesados++;
                auto tiempoActual = tiempoActual_ms();
                if((tiempoActual-marcaTiempoInicial)>1000){
                    marcaTiempoInicial = tiempoActual;
                    #ifdef QT_DEBUG
                    cout<<"Cuadros procesados en los ultimos 1 segundos: "<<cuadrosProcesados<<endl;
                    auto frameSize = pict.size();
                    cout<<"Ancho de imagen: "<<pict.cols<<endl;
                    cout<<"Alto de imagen: "<<pict.rows<<endl;
                    cout<<"Profundidad de imagen: "<<pict.depth()<<endl;
                    cout<<"Dimensiones de imagen: "<<pict.dims<<endl;
                    cout<<"imagenComprimida.size(): "<<imagenComprimida.size()<<endl;
                    cout<<endl;
                    #endif //QT_DEBUG
                    cuadrosProcesados=0;
                }
                video_checkTime(anchoImagen, altoImagen, fpsImagen, QDateTime::currentMSecsSinceEpoch());
                delete frame;                    
            }

            frame = nullptr;
        }
                             );
        Mat* cuadroCapturado = new Mat;;
        cap >> *cuadroCapturado; // get a new frame from camera
        hiloProc.join();
        frame = cuadroCapturado;

        //Versi hay comandosderecepcion
        int bytesRecibidos = recv(sockfd, &rcvBuffer[0], sizeof(rcvBuffer), 0);
        if(bytesRecibidos==sizeof(rcvBuffer)){
            if(rcvBuffer[0] == 0xFF){
                //transmitir = rcvBuffer[3];
                tx_webcam_lastRex = thisIterationTime;
            }
        }
        if((thisIterationTime-tx_webcam_lastRex)>500){
            tx_webcam_dly = 32;
        }else{
            tx_webcam_dly = 1;
        }
        tx_webcam_dly_ctr++;
        tx_webcam_dly_ctr%=tx_webcam_dly;

    }
    video_close();
    // the camera will be deinitialized automatically in VideoCapture destructor
    cout<<"FIN"<<endl;
    return 0;
    
}

