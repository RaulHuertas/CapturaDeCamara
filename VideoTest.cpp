
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

int main( int argc, char** argv )
{
    char* hostname;
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent* server;
    int serverlen;
    
    
    if(argc!=3){
        printf("Argumentos del programa invalidos\n");
        printf("Invocarlo como: Test2 <direccionDestino>  <puertoDestino>\n");
        exit(EXIT_FAILURE);
    }
    
    //CREANDO SOCKET UDP
    hostname = argv[1];
    portno = atoi(argv[2]);
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
    
    
    /// Load the source image
    VideoCapture cap(0);
    if(!cap.isOpened()){
        return -1;
    }
    cout<<"CAP_PROP_FRAME_WIDTH: "<<cap.get(CAP_PROP_FRAME_WIDTH)<<endl;
    cout<<"CAP_PROP_FRAME_HEIGHT: "<<cap.get(CAP_PROP_FRAME_HEIGHT)<<endl;
    cout<<"CAP_PROP_FPS: "<<cap.get(CAP_PROP_FPS)<<endl;
    cout<<"CAP_PROP_FOURCC: "<<cap.get(CAP_PROP_FOURCC)<<endl;
    int formatoStream = cap.get(CAP_PROP_FORMAT);
    cout<<"CAP_PROP_FORMAT: "<<formatoStream<<endl;
    cout<<"CAP_PROP_FORMAT ESQ: "<<((formatoStream==CV_8UC(3))?"true":"false")<<endl;

    Mat edges;
    namedWindow("edges",1);
    unsigned long long marcaTiempoInicial = tiempoActual_ms();
    int cuadrosProcesados = 0;
    for(;;)
    {
        Mat frame;
        cap >> frame; // get a new frame from camera
        //cvtColor(frame, edges, COLOR_BGR2GRAY);
        //GaussianBlur(edges, edges, Size(7,7), 1.5, 1.5);
        //Canny(edges, edges, 0, 30, 3);
        //imshow("edges", edges);
        cuadrosProcesados++;
        //Comprimir a JPEG
        std::vector<uchar> imagenComprimida;//buffer for coding
        imagenComprimida.reserve(1280*720*3);
        std::vector<int> parametrosCompresion(2);
        parametrosCompresion[0] = cv::IMWRITE_JPEG_QUALITY;
        parametrosCompresion[1] = 80;
        cv::imencode(".jpg", frame, imagenComprimida, parametrosCompresion);
        
	//TRANSMISION POR UDP
        transmitirPorUDP(frame.cols, frame.rows, formatoStream, imagenComprimida, sockfd, serveraddr, serverlen);
        
        auto tiempoActual = tiempoActual_ms();
        if((tiempoActual-marcaTiempoInicial)>1000){
            marcaTiempoInicial = tiempoActual;
            cout<<"Cuadros procesados en los ultimos 1 segundos: "<<cuadrosProcesados<<endl;
            auto frameSize = frame.size();
            cout<<"Ancho de imagen: "<<frame.cols<<endl;
            cout<<"Alto de imagen: "<<frame.rows<<endl;
            cout<<"Profundidad de imagen: "<<frame.depth()<<endl;
            cout<<"Dimensiones de imagen: "<<frame.dims<<endl;
            cout<<"CAP_PROP_FPS: "<<cap.get(CAP_PROP_FPS)<<endl;
            cout<<"imagenComprimida.size(): "<<imagenComprimida.size()<<endl;
            //cout<<"DATOS: "<<endl;
            //cout<<frame<<endl;
            cout<<endl;
            cuadrosProcesados=0;
        }
        
        
        
        
    }
    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;

}

