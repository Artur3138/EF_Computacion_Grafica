/*
* SIMULACION 3D DEL SISTEMA SOLAR - OpenGL + GLUT
* Archivos TGA sin compresion (24 o 32 bits):
*   sol.tga mercurio.tga venus.tga tierra.tga marte.tga jupiter.tga
*   saturno.tga urano.tga neptuno.tga luna.tga
*
* Controles:
*   W A S D   mover camara       Q / E   subir / bajar
*   Mouse     girar la vista     M       capturar / liberar mouse
*   P         pausar             + / -   aumentar / disminuir velocidad
*   O         mostrar / ocultar orbitas
*   1..8      enfocar Mercurio, Venus, Tierra, Marte, Jupiter, Saturno, Urano, Neptuno
*   R         volver a la vista inicial
*   Esc       salir
*/

#ifdef __APPLE__
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <GLUT/glut.h>
#else
# include <GL/gl.h>
# include <GL/glu.h>
# include <GL/glut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FPS 60
#define NUM_PLANETAS 8
#define NUM_ESTRELLAS 700
#define SEG_POR_ANIO 20.0f
#define SEG_POR_DIA 2.0f

/* ========================================================================
TEXTURAS TGA: formato TGA true-color sin compresion, 24 o 32 bits.
======================================================================== */

typedef struct {
	GLubyte* dibujo;
	GLuint bpp;
	GLuint largo;
	GLuint ancho;
	GLuint ID;
} textura;

int cargarTGA(const char* nombre, textura* imagen) {
	GLubyte cabeceraTGA[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	GLubyte compararTGA[12];
	GLubyte cabecera[6];
	GLuint bytesPorPixel, tamanoImagen, temp, i;
	GLenum formato = GL_RGBA;
	
	imagen->ID = 0;
	imagen->dibujo = NULL;
	
	FILE* archivo = fopen(nombre, "rb");
	if (!archivo) {
		printf("No se encontro el archivo de textura: %s\n", nombre);
		return 0;
	}
	
	if (fread(compararTGA, 1, sizeof(compararTGA), archivo) != sizeof(compararTGA) ||
		memcmp(cabeceraTGA, compararTGA, sizeof(cabeceraTGA)) != 0 ||
		fread(cabecera, 1, sizeof(cabecera), archivo) != sizeof(cabecera)) {
		printf("Formato TGA no soportado: %s\n", nombre);
		fclose(archivo);
		return 0;
	}
		
		imagen->ancho = 256 * cabecera[1] + cabecera[0];
		imagen->largo = 256 * cabecera[3] + cabecera[2];
		imagen->bpp = cabecera[4];
		
		if (imagen->ancho == 0 || imagen->largo == 0 ||
			(imagen->bpp != 24 && imagen->bpp != 32)) {
			printf("Datos invalidos en la cabecera TGA: %s\n", nombre);
			fclose(archivo);
			return 0;
		}
			
			bytesPorPixel = imagen->bpp / 8;
			tamanoImagen = imagen->largo * imagen->ancho * bytesPorPixel;
			imagen->dibujo = (GLubyte*)malloc(tamanoImagen);
			
			if (!imagen->dibujo ||
				fread(imagen->dibujo, 1, tamanoImagen, archivo) != tamanoImagen) {
				printf("Error leyendo la textura TGA: %s\n", nombre);
				free(imagen->dibujo);
				imagen->dibujo = NULL;
				fclose(archivo);
				return 0;
			}
			
			/* TGA almacena los colores como BGR(A); OpenGL espera RGB(A). */
			for (i = 0; i < tamanoImagen; i += bytesPorPixel) {
				temp = imagen->dibujo[i];
				imagen->dibujo[i] = imagen->dibujo[i + 2];
				imagen->dibujo[i + 2] = temp;
			}
			fclose(archivo);
			
			glGenTextures(1, &imagen->ID);
			glBindTexture(GL_TEXTURE_2D, imagen->ID);
			if (imagen->bpp == 24) formato = GL_RGB;
			
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, formato,
						 imagen->ancho, imagen->largo, 0,
						 formato, GL_UNSIGNED_BYTE, imagen->dibujo);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			
			free(imagen->dibujo);
			imagen->dibujo = NULL;
			
			printf("Textura cargada: %s (%u x %u, %u bits)\n",
				   nombre, imagen->ancho, imagen->largo, imagen->bpp);
			return 1;
}

/* ========================================================================
CUERPOS CELESTES
======================================================================== */

typedef struct {
	const char* nombre;
	const char* archivo;
	float r, g, b;                 /* color de respaldo si falta el TGA */
	float radio;
	float distancia;
	float periodoOrbital;          /* anios terrestres */
	float periodoRotacion;         /* dias terrestres */
	float inclinacion;             /* grados */
	float velOrbita;               /* grados por cuadro */
	float velRotacion;             /* grados por cuadro */
	float anguloOrbita;
	float anguloRotacion;
	textura tex;
	int tieneTex;
} cuerpo;

static float vOrb(float periodoAnios) {
	return 360.0f / (periodoAnios * SEG_POR_ANIO * FPS);
}

static float vRot(float periodoDias) {
	return 360.0f / (periodoDias * SEG_POR_DIA * FPS);
}

cuerpo sol = {
	"Sol", "sol.tga", 1.0f, 0.8f, 0.1f,
		30.0f, 0.0f, 0.0f, 25.4f, 7.25f,
		0.0f, vRot(25.4f), 0.0f, 0.0f
};

cuerpo planetas[NUM_PLANETAS] = {
	/* nombre     textura          RGB respaldo         radio dist periodoOrb rotacion inclinacion */
	{"Mercurio", "mercurio.tga", 0.60f,0.58f,0.55f,   3.0f,  60.0f,   0.24f,  58.60f,   0.03f, vOrb(0.24f),   vRot(58.60f),   0.0f, 0.0f},
{"Venus",    "venus.tga",    0.90f,0.75f,0.40f,   6.0f,  90.0f,   0.62f, 243.00f, 177.40f, vOrb(0.62f),   vRot(243.00f),  0.0f, 0.0f},
	{"Tierra",   "tierra.tga",   0.20f,0.40f,0.90f,   6.5f, 125.0f,   1.00f,   1.00f,  23.40f, vOrb(1.00f),   vRot(1.00f),    0.0f, 0.0f},
{"Marte",    "marte.tga",    0.80f,0.35f,0.20f,   4.0f, 160.0f,   1.88f,   1.03f,  25.20f, vOrb(1.88f),   vRot(1.03f),    0.0f, 0.0f},
	{"Jupiter",  "jupiter.tga",  0.80f,0.65f,0.50f,  16.0f, 260.0f,  11.86f,   0.41f,   3.10f, vOrb(11.86f),  vRot(0.41f),    0.0f, 0.0f},
{"Saturno",  "saturno.tga",  0.85f,0.78f,0.55f,  13.0f, 360.0f,  29.46f,   0.45f,  26.70f, vOrb(29.46f),  vRot(0.45f),    0.0f, 0.0f},
	{"Urano",    "urano.tga",    0.55f,0.80f,0.85f,   9.0f, 440.0f,  84.01f,   0.72f,  97.80f, vOrb(84.01f),  vRot(0.72f),    0.0f, 0.0f},
{"Neptuno",  "neptuno.tga",  0.25f,0.35f,0.90f,   9.0f, 510.0f, 164.80f,   0.67f,  28.30f, vOrb(164.80f), vRot(0.67f),    0.0f, 0.0f}
};

/* La Luna mantiene rotacion sincronica con su traslacion. */
cuerpo luna = {
	"Luna", "luna.tga", 0.75f,0.75f,0.75f,
		1.8f, 12.0f, 0.0748f, 27.3f, 6.7f,
		vOrb(0.0748f), vOrb(0.0748f), 0.0f, 0.0f
};

GLUquadric* quad = NULL;

/* ========================================================================
ESTADO GLOBAL
======================================================================== */

float escalaTiempo = 1.0f;
int pausa = 0;
int verOrbitas = 1;
int anchoVentana = 1280;
int altoVentana = 720;
float aniosSimulados = 0.0f;

float camX, camY, camZ;
float yaw, pitch;
float velCamara = 3.0f;
int capturaMouse = 1;
int primerMovimiento = 1;
int teclas[256] = {0};
int seguir = -1;                  /* -1 = camara libre */

float estrellas[NUM_ESTRELLAS][3];

/* ========================================================================
CAMARA
======================================================================== */

void direccion(float* dx, float* dy, float* dz) {
	float y = yaw * M_PI / 180.0f;
	float p = pitch * M_PI / 180.0f;
	*dx = cos(p) * cos(y);
	*dy = sin(p);
	*dz = cos(p) * sin(y);
}

void mirarA(float x, float y, float z) {
	float dx = x - camX;
	float dy = y - camY;
	float dz = z - camZ;
	float len = sqrt(dx*dx + dy*dy + dz*dz);
	if (len < 0.0001f) return;
	yaw = atan2(dz, dx) * 180.0f / M_PI;
	pitch = asin(dy / len) * 180.0f / M_PI;
}

void vistaInicial() {
	seguir = -1;
	camX = 0.0f;
	camY = 350.0f;
	camZ = 550.0f;
	mirarA(0.0f, 0.0f, 0.0f);
}

void posicionPlaneta(int i, float* x, float* y, float* z) {
	float a = planetas[i].anguloOrbita * M_PI / 180.0f;
	*x = planetas[i].distancia * cos(a);
	*y = 0.0f;
	*z = -planetas[i].distancia * sin(a);
}

/* Seguimiento dinamico: la camara permanece cerca del planeta y lo mira. */
void seguirPlaneta() {
	if (seguir < 0 || seguir >= NUM_PLANETAS) return;
	
	float px, py, pz;
	posicionPlaneta(seguir, &px, &py, &pz);
	
	float a = planetas[seguir].anguloOrbita * M_PI / 180.0f;
	float haciaSolX = -cos(a);
	float haciaSolZ =  sin(a);
	float tanX = -sin(a);
	float tanZ = -cos(a);
	
	float lx = 0.6f * haciaSolX + tanX;
	float lz = 0.6f * haciaSolZ + tanZ;
	float largo = sqrt(lx*lx + lz*lz);
	if (largo < 0.0001f) largo = 1.0f;
	lx /= largo;
	lz /= largo;
	
	float separacion = planetas[seguir].radio * 4.0f + 10.0f;
	camX = px + lx * separacion;
	camY = py + separacion * 0.35f;
	camZ = pz + lz * separacion;
	mirarA(px, py, pz);
}

void enfocar(int i) {
	if (i < 0 || i >= NUM_PLANETAS) return;
	seguir = i;
	seguirPlaneta();
}

/* ========================================================================
CARGA Y DIBUJO DE TEXTURAS
======================================================================== */

void cargarTexturaCuerpo(cuerpo* c) {
	c->tieneTex = cargarTGA(c->archivo, &c->tex);
}

void dibujarEsfera(cuerpo* c) {
	if (c->tieneTex) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, c->tex.ID);
		glColor3f(1.0f, 1.0f, 1.0f);
	} else {
		glDisable(GL_TEXTURE_2D);
		glColor3f(c->r, c->g, c->b);
	}
	
	glPushMatrix();
	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
	gluSphere(quad, c->radio, 48, 48);
	glPopMatrix();
	
	glDisable(GL_TEXTURE_2D);
}


void dibujarSol() {
	glDisable(GL_LIGHTING);
	glPushMatrix();
	glRotatef(sol.anguloRotacion, 0.0f, 1.0f, 0.0f);
	dibujarEsfera(&sol);
	glPopMatrix();
	
	/* Capas para simular el resplandor del Sol. */
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);
	
	const float capas[3][2] = {
		{1.12f, 0.25f},
	{1.30f, 0.12f},
		{1.60f, 0.05f}
	};
	
	for (int i = 0; i < 3; i++) {
		glColor4f(1.0f, 0.65f, 0.15f, capas[i][1]);
		gluSphere(quad, sol.radio * capas[i][0], 40, 24);
	}
	
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_LIGHTING);
}

void dibujarOrbita(float radio) {
	glBegin(GL_LINE_LOOP);
	for (int i = 0; i < 120; i++) {
		float a = 2.0f * M_PI * i / 120.0f;
		glVertex3f(radio * cos(a), 0.0f, radio * sin(a));
	}
	glEnd();
}

/* Anillos de Saturno */
void dibujarAnillos(float radioPlaneta) {
	const int segmentos = 100;
	const float franjas[5][6] = {
		{1.25f, 1.45f, 0.55f, 0.50f, 0.42f, 0.35f},
	{1.45f, 1.75f, 0.85f, 0.78f, 0.62f, 0.75f},
		{1.75f, 1.80f, 0.20f, 0.18f, 0.15f, 0.10f},
	{1.80f, 2.05f, 0.80f, 0.72f, 0.56f, 0.65f},
		{2.05f, 2.15f, 0.65f, 0.60f, 0.50f, 0.30f}
	};
	
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	for (int f = 0; f < 5; f++) {
		float rIn = franjas[f][0] * radioPlaneta;
		float rOut = franjas[f][1] * radioPlaneta;
		glColor4f(franjas[f][2], franjas[f][3], franjas[f][4], franjas[f][5]);
		glBegin(GL_QUAD_STRIP);
		for (int i = 0; i <= segmentos; i++) {
			float a = 2.0f * M_PI * i / segmentos;
			glVertex3f(rIn * cos(a), 0.0f, rIn * sin(a));
			glVertex3f(rOut * cos(a), 0.0f, rOut * sin(a));
		}
		glEnd();
	}
	
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);
	glColor3f(1.0f, 1.0f, 1.0f);
}

void dibujarEstrellas() {
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glColor3f(1.0f, 1.0f, 1.0f);
	glPointSize(1.5f);
	glBegin(GL_POINTS);
	for (int i = 0; i < NUM_ESTRELLAS; i++) {
		glVertex3f(estrellas[i][0], estrellas[i][1], estrellas[i][2]);
	}
	glEnd();
	glEnable(GL_LIGHTING);
}

void dibujarTodasLasOrbitas() {
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	for (int i = 0; i < NUM_PLANETAS; i++) {
		if (i == seguir) glColor4f(0.3f, 0.8f, 1.0f, 0.85f);
		else glColor4f(0.5f, 0.6f, 0.8f, 0.35f);
		dibujarOrbita(planetas[i].distancia);
	}
	
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);
}

/* ========================================================================
HUD: informacion del planeta enfocado
======================================================================== */

void escribir(float x, float y, const char* texto, void* fuente) {
	glRasterPos2f(x, y);
	for (const char* c = texto; *c; c++) {
		glutBitmapCharacter(fuente, *c);
	}
}

void dibujarHUD() {
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, anchoVentana, 0, altoVentana);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	
	char linea[180];
	float y = altoVentana - 25.0f;
	
	glColor3f(1.0f, 0.85f, 0.3f);
	escribir(15.0f, y, "SISTEMA SOLAR 3D", GLUT_BITMAP_HELVETICA_18);
	
	glColor3f(0.85f, 0.85f, 0.90f);
	sprintf(linea, "Velocidad: x%.2f   %s", escalaTiempo, pausa ? "[PAUSA]" : "");
	escribir(15.0f, y -= 24.0f, linea, GLUT_BITMAP_HELVETICA_12);
	
	sprintf(linea, "Tiempo simulado: %.2f anios terrestres", aniosSimulados);
	escribir(15.0f, y -= 18.0f, linea, GLUT_BITMAP_HELVETICA_12);
	
	sprintf(linea, "Orbitas: %s", verOrbitas ? "visibles" : "ocultas");
	escribir(15.0f, y -= 18.0f, linea, GLUT_BITMAP_HELVETICA_12);
	
	if (seguir >= 0 && seguir < NUM_PLANETAS) {
		cuerpo* p = &planetas[seguir];
		
		glColor3f(0.3f, 0.8f, 1.0f);
		sprintf(linea, "Enfocando: %s", p->nombre);
		escribir(15.0f, y -= 28.0f, linea, GLUT_BITMAP_HELVETICA_18);
		
		glColor3f(0.8f, 0.9f, 1.0f);
		sprintf(linea, "Periodo orbital: %.2f anios", p->periodoOrbital);
		escribir(15.0f, y -= 20.0f, linea, GLUT_BITMAP_HELVETICA_12);
		
		sprintf(linea, "Periodo de rotacion: %.2f dias%s", p->periodoRotacion,
				(seguir == 1 || seguir == 6) ? " (sentido retrogrado)" : "");
		escribir(15.0f, y -= 18.0f, linea, GLUT_BITMAP_HELVETICA_12);
		
		sprintf(linea, "Inclinacion del eje: %.2f grados", p->inclinacion);
		escribir(15.0f, y -= 18.0f, linea, GLUT_BITMAP_HELVETICA_12);
	}
	
	glColor3f(0.65f, 0.65f, 0.70f);
	escribir(15.0f, 15.0f,
			 "WASD/QE: mover | Mouse: mirar | P: pausa | +/-: velocidad | O: orbitas | "
			 "1-8: enfocar planeta | R: vista inicial | M: mouse | Esc: salir",
			 GLUT_BITMAP_HELVETICA_12);
	
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
}

/* ========================================================================
DIBUJO DE LOS PLANETAS Y LA LUNA
======================================================================== */

void dibujarPlaneta(int i) {
	cuerpo* p = &planetas[i];
	
	glPushMatrix();
	glRotatef(p->anguloOrbita, 0.0f, 1.0f, 0.0f);
	glTranslatef(p->distancia, 0.0f, 0.0f);
	
	glPushMatrix();
	glRotatef(p->inclinacion, 0.0f, 0.0f, 1.0f);
	glRotatef(p->anguloRotacion, 0.0f, 1.0f, 0.0f);
	dibujarEsfera(p);
	glPopMatrix();
	
	if (i == 5) { /* Saturno */
		glPushMatrix();
		glRotatef(p->inclinacion, 0.0f, 0.0f, 1.0f);
		dibujarAnillos(p->radio);
		glPopMatrix();
	}
	
	if (i == 2) { /* Luna alrededor de la Tierra */
		glRotatef(luna.anguloOrbita, 0.0f, 1.0f, 0.0f);
		glTranslatef(luna.distancia, 0.0f, 0.0f);
		glRotatef(luna.anguloRotacion, 0.0f, 1.0f, 0.0f);
		dibujarEsfera(&luna);
	}
	glPopMatrix();
}

/* ========================================================================
CAMARA LIBRE Y CONTROLES
======================================================================== */

void moverCamara() {
	float dx, dy, dz;
	direccion(&dx, &dy, &dz);
	
	float rx = -dz;
	float rz = dx;
	float largo = sqrt(rx*rx + rz*rz);
	if (largo > 0.0001f) {
		rx /= largo;
		rz /= largo;
	}
	
	int movio = 0;
	if (teclas['w']) { camX += dx*velCamara; camY += dy*velCamara; camZ += dz*velCamara; movio = 1; }
	if (teclas['s']) { camX -= dx*velCamara; camY -= dy*velCamara; camZ -= dz*velCamara; movio = 1; }
	if (teclas['d']) { camX += rx*velCamara; camZ += rz*velCamara; movio = 1; }
	if (teclas['a']) { camX -= rx*velCamara; camZ -= rz*velCamara; movio = 1; }
	if (teclas['q']) { camY += velCamara; movio = 1; }
	if (teclas['e']) { camY -= velCamara; movio = 1; }
	
	if (movio) seguir = -1;
}

void update(int valor) {
	float factor = pausa ? 0.0f : escalaTiempo;
	
	if (factor != 0.0f) {
		sol.anguloRotacion += sol.velRotacion * factor;
		
		for (int i = 0; i < NUM_PLANETAS; i++) {
			planetas[i].anguloOrbita += planetas[i].velOrbita * factor;
			planetas[i].anguloRotacion += planetas[i].velRotacion * factor;
			if (planetas[i].anguloOrbita >= 360.0f) planetas[i].anguloOrbita -= 360.0f;
			if (planetas[i].anguloRotacion >= 360.0f) planetas[i].anguloRotacion -= 360.0f;
		}
		
		luna.anguloOrbita += luna.velOrbita * factor;
		luna.anguloRotacion += luna.velRotacion * factor;
		if (luna.anguloOrbita >= 360.0f) luna.anguloOrbita -= 360.0f;
		if (luna.anguloRotacion >= 360.0f) luna.anguloRotacion -= 360.0f;
		if (sol.anguloRotacion >= 360.0f) sol.anguloRotacion -= 360.0f;
		
		aniosSimulados += (0.3f * factor) / 360.0f;
	}
	
	moverCamara();
	if (seguir >= 0) seguirPlaneta();
	
	glutPostRedisplay();
	glutTimerFunc(1000 / FPS, update, 0);
}

void keyboard_cb(unsigned char key, int x, int y) {
	key = (unsigned char)tolower(key);
	teclas[key] = 1;
	
	switch (key) {
	case 27:
		exit(0);
		break;
	case 'p':
		pausa = !pausa;
		break;
	case 'o':
		verOrbitas = !verOrbitas;
		break;
	case 'r':
		vistaInicial();
		break;
	case '+':
	case '=':
		escalaTiempo *= 1.5f;
		if (escalaTiempo > 200.0f) escalaTiempo = 200.0f;
		break;
	case '-':
	case '_':
		escalaTiempo /= 1.5f;
		if (escalaTiempo < 0.05f) escalaTiempo = 0.05f;
		break;
	case 'm':
		capturaMouse = !capturaMouse;
		primerMovimiento = 1;
		glutSetCursor(capturaMouse ? GLUT_CURSOR_NONE : GLUT_CURSOR_INHERIT);
		break;
	default:
		if (key >= '1' && key <= '8') enfocar(key - '1');
		break;
	}
	glutPostRedisplay();
}

void keyboardUp_cb(unsigned char key, int x, int y) {
	teclas[(unsigned char)tolower(key)] = 0;
}

void mouse_cb(int x, int y) {
	if (!capturaMouse || seguir >= 0) return;
	
	int cx = anchoVentana / 2;
	int cy = altoVentana / 2;
	int dx = x - cx;
	int dy = y - cy;
	if (dx == 0 && dy == 0) return;
	
	if (primerMovimiento) {
		primerMovimiento = 0;
		glutWarpPointer(cx, cy);
		return;
	}
	
	yaw += dx * 0.15f;
	pitch -= dy * 0.15f;
	if (pitch > 89.0f) pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;
	glutWarpPointer(cx, cy);
}

/* ========================================================================
INICIALIZACION OPENGL, LUZ, ESTRELLAS Y TEXTURAS
======================================================================== */

void reshape_cb(int w, int h) {
	if (w <= 0 || h <= 0) return;
	anchoVentana = w;
	altoVentana = h;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, (double)w / (double)h, 0.5, 3000.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void init_escena() {
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
	glClearDepth(1.0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_NORMALIZE);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
	GLfloat ambiente[] = {0.10f, 0.10f, 0.10f, 1.0f};
	GLfloat difusa[] = {1.0f, 1.0f, 0.95f, 1.0f};
	GLfloat especular[] = {0.0f, 0.0f, 0.0f, 1.0f};
	
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambiente);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, difusa);
	glLightfv(GL_LIGHT0, GL_SPECULAR, especular);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	quad = gluNewQuadric();
	gluQuadricTexture(quad, GL_TRUE);
	gluQuadricNormals(quad, GLU_SMOOTH);
	
	printf("\nCargando texturas TGA...\n");
	cargarTexturaCuerpo(&sol);
	for (int i = 0; i < NUM_PLANETAS; i++) cargarTexturaCuerpo(&planetas[i]);
	cargarTexturaCuerpo(&luna);
	printf("\nSi una textura no se encuentra, ese cuerpo aparecera con color solido.\n");
	
	srand(7);
	for (int i = 0; i < NUM_ESTRELLAS; i++) {
		float a = 2.0f * M_PI * (float)rand() / (float)RAND_MAX;
		float b = acos(2.0f * (float)rand() / (float)RAND_MAX - 1.0f);
		float R = 1500.0f;
		estrellas[i][0] = R * sin(b) * cos(a);
		estrellas[i][1] = R * cos(b);
		estrellas[i][2] = R * sin(b) * sin(a);
	}
	
	vistaInicial();
}

void display_cb() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	float dx, dy, dz;
	direccion(&dx, &dy, &dz);
	gluLookAt(camX, camY, camZ,
			  camX + dx, camY + dy, camZ + dz,
			  0.0, 1.0, 0.0);
	
	/* La luz se coloca en el origen, donde esta el Sol. */
	GLfloat posLuz[] = {0.0f, 0.0f, 0.0f, 1.0f};
	glLightfv(GL_LIGHT0, GL_POSITION, posLuz);
	
	dibujarEstrellas();
	
	if (verOrbitas) dibujarTodasLasOrbitas();
	
	dibujarSol();
	
	for (int i = 0; i < NUM_PLANETAS; i++) dibujarPlaneta(i);
	
	dibujarHUD();
	glutSwapBuffers();
}

void initialize() {
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(anchoVentana, altoVentana);
	glutInitWindowPosition(100, 50);
	glutCreateWindow("Simulacion 3D del Sistema Solar - Texturas y HUD");
	
	glutIgnoreKeyRepeat(1);
	
	init_escena();
	
	glutDisplayFunc(display_cb);
	glutReshapeFunc(reshape_cb);
	glutKeyboardFunc(keyboard_cb);
	glutKeyboardUpFunc(keyboardUp_cb);
	glutPassiveMotionFunc(mouse_cb);
	glutMotionFunc(mouse_cb);
	
	glutTimerFunc(1000 / FPS, update, 0);
	
	glutSetCursor(GLUT_CURSOR_NONE);
	
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	
	initialize();
	
	printf("\nSimulacion iniciada. Presiona Esc para salir.\n");
	
	glutMainLoop();
	
	return 0;
}
