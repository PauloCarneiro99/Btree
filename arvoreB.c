/*
   Fabio Fogarin Destro 10284667
   Paulo Andre de Oliveira Carneiro 10295304 
   Renata Vinhaga dos Anjos 10295263 
   Vitor Henrique Gratiere Torres 10284952 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define TAM_CAMPO 10
#define TAM_REG 112
#define TAM_CABECALHO 5
#define ORDEM 10 //ordem da arvore B
#define MAX_CHAVES ORDEM-1
#define TAM_PAG 116 //tamanho da pagina do indice
#define TAM_CABECALHO_B 13
#define TAM_BUFFER 5
#define NIL -1
#define t ORDEM/2

// registro de cabeçalho
typedef struct{
	char  status;
	int  topoPilha;
}Cabecalho;

// registro de dados
typedef struct{
	int  codEscola;
	char  *dataInicio;
	char  *dataFinal;
	int  tamEscola;
	char  *nomeEscola;
	int  tamMunicipio;
	char  *municipio;
	int  tamEndereco;
	char  *endereco;
}Escola;

//Cabecalho do arquivo de indice arvore B - 13 bytes
typedef struct{
	char status;//status do arquivo de indice - 0 ou 1
	int noRaiz;//RRN do nó (página) raiz do indice arvore B;
	int altura;//altura da arvore = altura do nó raiz;
	int ultimoRRN;// último RRN alocado pelo índice árvore-B
}Cabecalho_B;

/*struct com chave de busca (codEscola) e seu campo 
de referência para o registro no arquivo de dados*/
typedef struct{
	int chave, RRN;
}C_PR;

//nó da arvore B = página (116 bytes)
typedef struct{
	int N;//numero de chaves na pagina
	C_PR** c_pr;//par chave + RRN no arquivo de dados
	int* P;//"Ponteiros" das paginas filhas, -1 aponta para NULL
}Pagina;

typedef struct
{
	int BufferMiss;
	int BufferHit;
	int RRN[TAM_BUFFER];
	char modificado[TAM_BUFFER];
	Pagina *node[TAM_BUFFER];
}BufferPool;

void print_Pagina();

Cabecalho_B* le_cabecalho_B(FILE* fp){
	Cabecalho_B* C = calloc(1 ,sizeof(Cabecalho_B));
	fseek(fp, 0, SEEK_SET);
	fread(&C->status, sizeof(char), 1, fp);
	fread(&C->noRaiz, sizeof(int), 1, fp);
	fread(&C->altura, sizeof(int), 1, fp);
	fread(&C->ultimoRRN, sizeof(int), 1, fp);
	fseek(fp, 0, SEEK_SET);
	return C;
}

void cria_Cabecalho_B(FILE* fp){
	Cabecalho_B* C = calloc(1, sizeof(Cabecalho_B));
	C->status = 0;
	C->noRaiz = -1;
	C->altura = 0;
	C->ultimoRRN = -1;

	fseek(fp, 0, SEEK_SET);
	fwrite(&C->status, sizeof(char), 1, fp);
	fwrite(&C->noRaiz, sizeof(int), 1, fp);
	fwrite(&C->altura, sizeof(int), 1, fp);
	fwrite(&C->ultimoRRN, sizeof(int), 1, fp);
	free(C);
}

//funcao que maloca uma pagina nova e inicializa seus valores
Pagina* cria_pagina(){
	Pagina* p = calloc(1 ,sizeof(Pagina));
	p->c_pr = calloc(1, sizeof(C_PR*) * (MAX_CHAVES));
	for(int i = 0; i < MAX_CHAVES; i++){
		p->c_pr[i] = calloc(1, sizeof(C_PR));
		p->c_pr[i]->chave = 0;
		p->c_pr[i]->RRN = -1;
	}
	p->P = calloc(1, sizeof(int) * ORDEM);
	p->N = 0;//numero de chaves de um nó recém criado
	memset(p->P, -1, sizeof(int) * ORDEM);
	return p;
}

//carregando um pagina de disco
Pagina* le_pagina(FILE* fp){
	Pagina* p = cria_pagina();
	fread(&p->N, sizeof(int), 1, fp); 
	for(int i = 0; i < MAX_CHAVES; i++){
		fread(&p->c_pr[i]->chave, sizeof(int), 1, fp);
		fread(&p->c_pr[i]->RRN, sizeof(int), 1, fp);
	}
	for(int i = 0; i < ORDEM; i++){
		fread(&p->P[i], sizeof(int), 1, fp);
	}
	return p;
}

//escreve uma pagina de disco no arquivo de Indice
void escreve_pagina(FILE* fp, Pagina* p){
	fwrite(&p->N, sizeof(int), 1, fp); 
	for(int i = 0; i < MAX_CHAVES; i++){
		fwrite(&p->c_pr[i]->chave, sizeof(int), 1, fp);
		fwrite(&p->c_pr[i]->RRN, sizeof(int), 1, fp);
	}
	for(int i = 0; i < ORDEM; i++){
		fwrite(&p->P[i], sizeof(int), 1, fp);
	}
}

//inicializando os valores do Buffer pool
void iniciaBufferPool(BufferPool *bp){
	bp->BufferMiss = 0;
	bp->BufferHit = 0;
	for(int i=0 ; i<TAM_BUFFER; i++){
		bp->modificado[i] = 0;
		bp->RRN[i] = -1;
		bp->node[i] = malloc(sizeof(Pagina));
	}
}

void paginaCopy(Pagina *a, Pagina *b){ // copia o conteudo da página B para a página A
	a->N = b->N;
	a->c_pr = b->c_pr;
	a->P = b->P;
}

void swap(BufferPool *bp, int i){
	Pagina *p = malloc(sizeof(Pagina));

	*p = *bp->node[i];
	*bp->node[i] = *bp->node[i+1];
	*bp->node[i+1] = *p;


	int RRN = bp->RRN[i];
	bp->RRN[i] = bp->RRN[i+1];
	bp->RRN[i+1] = RRN;

	char c = bp->modificado[i];	
	bp->modificado[i] = bp->modificado[i+1];
	bp->modificado[i+1] = c;
}

//Essa funçao eh responsavel por reorganizar o buffer pool, apos um acesso
//para que assim o elemento utilizado mais recentemente se encontre na ultima posição e o menos utilizado na posição 1
//e a raiz continua sempre na posição 0
int reorganiza(BufferPool *bp, int i){
	while(i < TAM_BUFFER-1){
		if(bp->RRN[i+1] == -1)
			break;
		swap(bp, i);
		i++;
	}
	return i;
}

//Percorre o buffer pool e atualiza no arquivo as paginas que foram modificadas no buffer pool
//função eh chamada no final da funcionalidade que estiver sendo executada
void flush(FILE *fp, BufferPool *bp){
	for(int i=0; i<TAM_BUFFER; i++){
		if(bp->RRN[i] == -1)
			break;
		if(bp->modificado[i] == 1){
			//inserir essa pagina no arquivo
			int offset = (TAM_PAG * bp->RRN[i])+ TAM_CABECALHO_B; //como calcular esse offset se nao tenho o RRN da pagina
			fseek(fp, offset, SEEK_SET);
			escreve_pagina(fp, bp->node[i]);
			bp->modificado[i] = 0;
		}
	}

	printf("PRINT DE TESTE NA FUNÇÃO FLUSH ::: buffer miss = %d  buffer hit = %d\n",bp->BufferMiss, bp->BufferHit);
	

	//salvando a taxa de buffer miss e buffer hit no final de um arquivo
	FILE *fe = fopen("buffer-info.text", "a");
	char *Buffer;
	if(fe == NULL){
		printf("Falha no carregamento do arquivo.\n");
		return;
	}
	Buffer = "Page fault:\0";
	fwrite(Buffer, sizeof(char)*strlen(Buffer), 1, fe);
	fwrite(&bp->BufferMiss, sizeof(int), 1, fe);

	Buffer = "Page hit:\0";
	fwrite(Buffer, sizeof(char)*strlen(Buffer), 1, fe);
	fwrite(&bp->BufferHit, sizeof(int), 1, fe);

	Buffer = "\n\0";
	fwrite(Buffer, sizeof(char)*strlen(Buffer), 1, fe);
	fclose(fe);
}

//insere uma nova pagina no buffer caso a mesma ainda nao esteja la (Buffer Miss), caso o contrario atualiza a pagina do buffer (Buffer Hit)
//Em caso de uma inserção na qual o buffer esta lotado, retira uma das paginas seguindo uma politica de substituição, que nesse caso
//adotamos a politica LRU, isto é , removemos a pagina que foi recentemente foi menos utilizada
//(devido a funcao reorganiza, a pagina recentemente menos utilizada esta na ṕosição 1, logo removo essa pagina, atualizo o arquivo caso seja necessario, insiro a
//nova pagina nessa posição e reorganiza o buffer)
void put(FILE*fp, int RRN, BufferPool *bp, Pagina* p){
	int flag = 0;
	int i;
	for(i=0; i<TAM_BUFFER; i++){
		if(bp->RRN[i] == -1)
			break;
		if(bp->RRN[i] == RRN){
			flag = 1;
			break;
		}
	}
	if(flag == 0){ //nao achei o RRN desejado no buffer, tenho que inseri-lo
		bp->BufferMiss += 1;
		if(bp->RRN[i] == -1){ //o buffer nao estava lotado, insirerindo na primeira posicao livre
			*bp->node[i] = *p;
			bp->modificado[i] = 0;
			bp->RRN[i] = RRN;
			fseek(fp, (TAM_PAG*RRN)+ TAM_CABECALHO_B, SEEK_SET);
			escreve_pagina(fp, p);
		}else{//o buffer esta lotado, logo sigo a politica de substituicao LRU, tirando o elemento menos utilizado que esta na posicao 1 e inserindo o novo e reorganizando
			if(bp->modificado[1] == 1){
				//pepgar o RRN do no que esta sendo removido
				//fazer um fseek para essa posicao
				//e atualizar oq ta escrito
				fseek(fp, (TAM_PAG*bp->RRN[1])+ TAM_CABECALHO_B, SEEK_SET);
				escreve_pagina(fp, bp->node[1]);
			}
			*bp->node[1] = *p;
			bp->modificado[1] = 0;
			bp->RRN[1] = RRN;
		
			reorganiza(bp, 1);
			
			 fseek(fp, (TAM_PAG*RRN)+ TAM_CABECALHO_B, SEEK_SET);
			 escreve_pagina(fp, p);
		}
	}else{//atualiza a informacao que ja esta no Buffer Pool
		//atualizar a informacao
		//marcar como pagina modificada
		//reorganizar	
		bp->BufferHit += 1;
		*bp->node[i] = *p;
		bp->modificado[i] = 1;
		if(i!=0) 
			reorganiza(bp,i);
	}
}

//em casos de split na raiz, chamamos essa função que atualiza a raiz no buffer pool
//isto é checa a posição 0 do buffer, se esta estiver modificada insira no arquivo, e atualize essa posição para a nova raiz
void modificaRaizBuffer(FILE *fp,int RRN, Pagina *p, BufferPool *bp){
	bp->BufferMiss += 1; 

	if(bp->RRN[0] != -1 && bp->modificado[0] == 1){ //salvando a antiga raiz no arquivo
		fseek(fp, (TAM_PAG*bp->RRN[0])+TAM_CABECALHO_B, SEEK_SET);
		escreve_pagina(fp, bp->node[0]);
	}


	bp->modificado[0] = 0;
	*bp->node[0] = *p;


	bp->RRN[0] = RRN;

	fseek(fp, (TAM_PAG*RRN)+ TAM_CABECALHO_B, SEEK_SET);
	escreve_pagina(fp, p);


	for(int i=1; i<TAM_BUFFER; i++){ //evitando que o RRN esteja inserido duas vezes no mesmo buffer pool
		if(RRN == bp->RRN[i]){
			bp->RRN[i] = -1;
			reorganiza(bp, i);
		}
	}	

}


//Buscando uma pagina no buffer, caso a mesma ja esteja inserida retorna o seu conteudo(Buffer Hit), caso o contrario, chama a funcao put, que ira inserir a nova pagina
//no buffer, e depois retorne o conteudo dessa nova pagina (Buffer Miss)
Pagina get(FILE *fp,int RRN, BufferPool *bp){
	for(int i=0; i<TAM_BUFFER; i++){
		if(bp->RRN[i] == -1)
			break;
		if(bp->RRN[i] == RRN){
			bp->BufferHit += 1;
			//entao devo reorganizar o vetor de buffer pool, fazer um swaap entre a posicao i e a posicao i-1, a nao ser que i seja 0, pois nao pode-se fazer swap com a raiz
			if(i != 0)//a raiz deve sempre estar na posicao 0
				i = reorganiza(bp, i);
			return *bp->node[i];
		}	

	}
	//se chegou aqui, nao achou o RRN entao devo procurar no arquivo e aplicar a politica de subsituicao no buffer pool
	Pagina *p;
	fseek(fp, (RRN*TAM_PAG)+TAM_CABECALHO_B,SEEK_SET);
	p = le_pagina(fp);
	put(fp, RRN, bp, p);
	return *p;
}


void  atualiza_Cabecalho_B(FILE *f, Cabecalho_B* C){
	fseek(f, 0, SEEK_SET); // garante que o byte offset esteja no inicio do arquivo
	fwrite(&C->status, sizeof(char), 1, f);
	fwrite(&C->noRaiz, sizeof(int), 1, f);
	fwrite(&C->altura, sizeof(int), 1, f);
	fwrite(&C->ultimoRRN, sizeof(int), 1, f);
	fseek(f, 0, SEEK_SET); // volta o byte offset para o inicio
}

void atualiza_status_B(FILE* f, char status){
	fseek(f, 0, SEEK_SET);
	fwrite(&status, sizeof(char), 1, f);
	fseek(f, 0, SEEK_SET); // volta o byte offset para o inicio
}

char verifica_status_B(FILE *fp){
	char status;
	fseek(fp,0,SEEK_SET); //garantindo que o ponteiro de arquivo esta no inicio
	fread(&status, sizeof(char),1,fp);
	if(status == 0)
		printf("Falha no processamento do arquivo.\n");
	fseek(fp,0,SEEK_SET); //retornando o ponteiro de arquivo para o inicio do arquivo
	return status;
}

void imprime_Cabecalho(Cabecalho_B* C){
	printf("status: %d ", C->status);
	printf("Raiz: %d ", C->noRaiz);
	printf("altura: %d ", C->altura);
	printf("ultimo RRN: %d ", C->ultimoRRN);
	printf("\n");
}


void cria_arvore(FILE* fp, int chave, int RRN_dados, BufferPool *bp){
	Pagina* p = cria_pagina();//cria uma pagina
	p->c_pr[0]->chave = chave;
	p->c_pr[0]->RRN = RRN_dados;
	p->N++; 
	modificaRaizBuffer(fp, 0, p, bp);
}


//percorre o vetor de ponteiros filhos da página e verifica 
//se existe pelo menos um filho, caso contrario é nó folha
int EhFolha(Pagina* p){
	for(int i = 0; i < ORDEM; i++){
		if(p->P[i] != -1){
			return 0;
		}
	}
	return 1;
}

void print_Pagina(Pagina* p){
	printf("%d ", p->N);
	for(int i = 0; i < MAX_CHAVES; i++){
		printf("%d ", p->c_pr[i]->chave);
		printf("%d ", p->c_pr[i]->RRN);
	}
	printf("||");
	for(int i = 0; i < ORDEM; i++){
		printf("%d ", p->P[i]);
	}
	printf("\n");
}

//Essa função é responsável pelo split de uma pagina cheia, promovendo uma das chaves e dividindo a pagina cheia em duas.
void split(FILE* fp, Cabecalho_B* C, Pagina *s, Pagina *r,  int i, int RRN_pai, BufferPool * bp, int RRN_atual){
	//r é a antiga raiz
	//s é a nova pai
	//z é o novo filho
	//nova pagina que vai guardar metade dos filhos de r
	Pagina* z = cria_pagina();
	z->N = t - 1;
	for(int j = 0; j < t - 1 ; j++){
		z->c_pr[j]->chave = r->c_pr[j+t]->chave;
		z->c_pr[j]->RRN = r->c_pr[j+t]->RRN;
		r->c_pr[j+t]->chave = 0;
		r->c_pr[j+t]->RRN = -1;
	}
	if(!EhFolha(r)){
		for(int j = 0; j < t; j++){
			z->P[j] = r->P[j+t];
			r->P[j+t] = -1;
		} 
	}
	r->N = t - 1;
	for(int j = s->N; j >= i + 1; j--){
		s->P[j+1] = s->P[j];
	}
	C->ultimoRRN+=1;
	s->P[i+1] = C->ultimoRRN; // s filho = z

	for(int j = s->N - 1; j >= i; j--){
		s->c_pr[j+1]->chave = s->c_pr[j]->chave;
		s->c_pr[j+1]->RRN = s->c_pr[j]->RRN;
	}
	s->c_pr[i]->chave = r->c_pr[t-1]->chave;
	s->c_pr[i]->RRN = r->c_pr[t-1]->RRN;
	r->c_pr[t-1]->chave = 0;
	r->c_pr[t-1]->RRN = -1;
	s->N++;

	put(fp,RRN_atual,bp,r);
	put(fp, C->ultimoRRN,bp,z);    
	put(fp, RRN_pai, bp, s);
}

//funcao percorre a arvore procurando a posição correta para inserir, e enquanto faz isso, checa se as paginas estao cheias, executando um split preventivo
void Insert_Non_Full(FILE* fp, Cabecalho_B* C, Pagina* s, int chave, int RRN_dados, int RRN_pai, BufferPool *bp){
	int offset;
	int i = s->N - 1;//inicializando i com o elemento mais a direita 

	if(EhFolha(s)){	//se o nó é folha

		//Devemos encontrar a posição correta para inserir a chave 
		//movendo as chaves e RRN uma posicao a frente
		while(i >= 0 && chave < s->c_pr[i]->chave){
			s->c_pr[i+1]->chave = s->c_pr[i]->chave;
			s->c_pr[i+1]->RRN = s->c_pr[i]->RRN;
			i--;
		}
		s->c_pr[i+1]->chave = chave;
		s->c_pr[i+1]->RRN = RRN_dados;
		s->N++;
		put(fp, RRN_pai, bp, s);

		return;
	}
	else{
		//fazendo uma busca sequencial, procuramos o filho em que deve estar a chave
		while(i >= 0 && chave < s->c_pr[i]->chave){
			i--;
		}
		i++;
		offset = s->P[i];
		
		Pagina* r = cria_pagina();
		*r = get(fp, offset, bp);

		//se a pagina esta cheia, split
		if(r->N == MAX_CHAVES){
			split(fp, C, s, r, i, RRN_pai, bp, s->P[i]);
			imprime_Cabecalho(C);
			if(chave > s->c_pr[i]->chave){
				i++;
			}
			offset = s->P[i];
			*r = get(fp, offset, bp);
		}
		Insert_Non_Full(fp, C, r, chave, RRN_dados, offset, bp);
	}
}


void Btree_Insert(FILE* fp, int chave, int RRN_dados, BufferPool *bp){
	Cabecalho_B* C;
	Pagina *r, *s;
	r = cria_pagina();
	s = cria_pagina();
	C = le_cabecalho_B(fp);

	if(C->noRaiz == NIL){//se a arvore esta vazia
		cria_arvore(fp, chave, RRN_dados, bp);
		//atualizando cabeçalho
		C->altura++;
		C->noRaiz = 0;
		C->ultimoRRN++;
		atualiza_Cabecalho_B(fp, C);
		free(C);
	}
	else{
		*r = get(fp, C->noRaiz, bp);				
		if(r->N == MAX_CHAVES){//se a raiz esta cheiam, split preventivo
			s = cria_pagina();
			s->P[0] = C->noRaiz;
			split(fp, C, s, r, 0, C->ultimoRRN + 2, bp, C->noRaiz);
			C->ultimoRRN++;
			C->altura++; //toda vez que é dado um split na raiz a altura aumenta em um nivel
			C->noRaiz = C->ultimoRRN; // apenas se foi dado split o RRN da raiz muda
			modificaRaizBuffer(fp, C->noRaiz,s, bp);//como foi feito um split na raiz, atualizo a raiz no buffer pool
			Insert_Non_Full(fp, C, s, chave,RRN_dados, C->noRaiz, bp);
			atualiza_Cabecalho_B(fp, C);
		}
		else{
			Insert_Non_Full(fp, C, r, chave, RRN_dados, C->noRaiz, bp);
			atualiza_Cabecalho_B(fp, C);
		}
	}
}

//Realiza uma busca pelo codEscola no arquivo de indice arvore B com auxilio do Buffer Pool
int buscaArvoreB(int chave){
	FILE* fp = fopen("indice.bin", "rb");
	if(fp == NULL){
		printf("Falha no carregamento do arquivo.\n");
		return -1;
	}
	Cabecalho_B* C  = le_cabecalho_B(fp);
	Pagina *p = cria_pagina();
	BufferPool bp;
	int i;
	int RRN;
	int flag;
	iniciaBufferPool(&bp);
	RRN = C->noRaiz;
	fseek(fp, (RRN*TAM_PAG)+TAM_CABECALHO_B, SEEK_SET);
	p = le_pagina(fp); //toda busca comeca pela pagina da raiz
	modificaRaizBuffer(fp,RRN,p ,&bp);
	while(!EhFolha(p)){
		flag = 0;
		for(i=0; i<p->N; i++){
			if(p->c_pr[i]->chave == chave){
				flush(fp, &bp);
				fclose(fp);
				return p->c_pr[i]->RRN;
			}
			if(p->c_pr[i]->chave > chave){
				RRN = p->P[i];
				*p = get(fp, RRN, &bp);
				flag = 1;
				break;
			}
		}
		if(flag == 0){
			RRN = p->P[i];
			*p = get(fp, RRN, &bp);
		}
	}
	//cheguei aqui entao to procurando em um no folha
	for(i=0; i<p->N; i++){
		if(p->c_pr[i]->chave == chave){
			flush(fp, &bp);
			fclose(fp);
			return p->c_pr[i]->RRN;
		}
		if(p->c_pr[i]->chave > chave){
			break;
		}
	}
	flush(fp, &bp);
	fclose(fp);
	return -1;//se chegou aqui, nao achou a chave buscada
}

int existencia_registro();
Escola* le_bin_escola();
void print_escola();
void free_escola();

void busca(int chave){
	Escola* r;
	int RRN = buscaArvoreB(chave);
	if(RRN == -1){
		printf("Chave nao encontrada\n");
		return;
	}
	FILE *fp = fopen("saida.bin", "rb");
	if(fp == NULL){
		printf("Falha no carregamento do arquivo.\n");
		return;
	}
	fseek(fp, (RRN*TAM_REG)+TAM_CABECALHO, SEEK_SET);
	if(existencia_registro(fp)){ //checo se o registro é valido, leio o registro e imprimo o seu conteudo
		r = le_bin_escola(fp);
		print_escola(r);
		free_escola(r);
	}else{ //registro invalido (registro removido)
		printf("Registro inexistente.\n");
	}
}


void imprime_indice(){
	FILE *f = fopen("indice.bin", "rb");
	Pagina* p;
	fseek(f, 0, SEEK_END);
	int tam = ftell(f);
	fseek(f, TAM_CABECALHO_B, SEEK_SET);
	while(ftell(f) < tam){
		p = le_pagina(f);
		print_Pagina(p);
	}
	fclose(f);
	free(p);
}
void atualizaStatus(FILE *f, char status){
	fseek(f, 0, SEEK_SET); // garante que o byte offset esteja no inicio do arquivo
	fwrite(&status, sizeof(char), 1, f);
	fseek(f, 0, SEEK_SET); // volta o byte offset para o inicio
}

char verifica_status(FILE *fp){
	char status;
	fseek(fp,0,SEEK_SET); //garantindo que o ponteiro de arquivo esta no inicio
	fread(&status, sizeof(char),1,fp);
	if(status == 0)
		printf("Falha no processamento do arquivo.\n");
	fseek(fp,0,SEEK_SET); //retornando o ponteiro de arquivo para o inicio do arquivo
	return status;
}

void insere_pilha(int novoTopo, FILE *fp){
	fseek(fp, 1, SEEK_SET);
	// escreve o novo topo
	fwrite(&novoTopo, sizeof(int), 1, fp);
}

void criaCabecalho(FILE *f){
	char status = 0;
	fwrite(&status, sizeof(char), 1, f);
	int topPilha = -1;
	fwrite(&topPilha, sizeof(int), 1, f);//define o topo da pilha '-1'
}

// Retorna o topo da pilha
int topoPilha(FILE *fp){
	int topo;
	fseek(fp, 1, SEEK_SET);
	fread(&topo, sizeof(int), 1, fp);
	fseek(fp, 0, SEEK_SET);
	return topo;
}

// verifica se existe um registro a partir da posição atual do ponteiro para o arquivo de dados, se existe retorna 1, senão 0
int existencia_registro(FILE *f){
	int aux;
	fread(&aux, sizeof(int), 1, f);
	fseek(f, -sizeof(int), SEEK_CUR);
	if(aux == -1){
		return 0;
	}
	return 1;
}

// Imprime todos os campos de um registro Escola. Quando os campos variáveis são nulos, apenas imprime seu tamanho (zero no caso) considera que codEscola nunca será nulo
void print_escola(Escola* r){
	printf("%d ", r->codEscola);
	printf("%s ", r->dataInicio);
	printf("%s ", r->dataFinal);
	printf("%d ", r->tamEscola);
	if(r->tamEscola != 0)
		printf("%s ", r->nomeEscola);
	printf("%d ", r->tamMunicipio);
	if(r->tamMunicipio != 0)
		printf("%s ", r->municipio);
	printf("%d ", r->tamEndereco);
	if(r->tamEndereco != 0)
		printf("%s ", r->endereco);
	printf("\n");
}

// Retorna um registro Escola a partir de uma linha lida do CSV
Escola *parse_linha(char *linha){
	char *str;
	int i = 0;
	char *vazio = "0000000000";
	Escola *r = malloc(sizeof(Escola));

	sscanf(linha + i, "%m[^;]", &str); // percorre a string até encontrar um ';'
	r->codEscola = atoi(str);
	i += strlen(str) + 1; // strlen + 1 por conta da ';'
	free(str);

	sscanf(linha + i, "%m[^;]", &str);
	r->nomeEscola = str;
	if(str){
		r->tamEscola = strlen(str);
		i += strlen(str) + 1;
	}
	else{
		r->tamEscola = 0;
		i++;
	}

	sscanf(linha + i, "%m[^;]", &str);
	r->municipio = str;
	if(str){
		r->tamMunicipio = strlen(str);
		i += strlen(str) + 1;
	}
	else{
		r->tamMunicipio = 0;
		i++;
	}

	sscanf(linha + i, "%m[^;]", &str);
	r->endereco = str;
	if(str){
		r->tamEndereco = strlen(str);
		i += strlen(str) + 1;
	}
	else{
		r->tamEndereco = 0;
		i++;
	}

	sscanf(linha + i, "%m[^;]", &str);
	if(str){
		r->dataInicio = str;
		i += TAM_CAMPO + 1;
	}
	else{
		r->dataInicio = strdup(vazio); // strdup copia e já aloca um espaço de memória
		i++;
	}

	sscanf(linha + i, "%m[^\n\r]", &str);
	if(str){
		r->dataFinal = str;
		i += TAM_CAMPO + 1;
	}
	else{
		r->dataFinal = strdup(vazio);
		i++;
	}

	return r;
}

// Escreve um registro Escola em um arquivo binario na ordem especificada pelo trab
void escreve_bin_escola(FILE *fp, Escola *r){
	int tamReg1, tamReg2, res;

	tamReg1 = ftell(fp);

	fwrite(&r->codEscola, sizeof(int), 1, fp);
	fwrite(r->dataInicio, sizeof(char), TAM_CAMPO, fp);
	fwrite(r->dataFinal, sizeof(char), TAM_CAMPO, fp);

	fwrite(&r->tamEscola, sizeof(int), 1, fp);
	if(r->tamEscola != 0)
		fwrite(r->nomeEscola, sizeof(char), r->tamEscola, fp);

	fwrite(&r->tamMunicipio, sizeof(int), 1, fp);
	if(r->tamMunicipio != 0)
		fwrite(r->municipio, sizeof(char), r->tamMunicipio, fp);

	fwrite(&r->tamEndereco, sizeof(int), 1, fp);
	if(r->tamEndereco != 0)
		fwrite(r->endereco, sizeof(char), r->tamEndereco, fp);

	tamReg2 = ftell(fp);
	res = TAM_REG - (tamReg2 - tamReg1); // calcula quantos bytes faltam para que o registro tenha 112 bytes

	// completa os bytes que faltam no registro com zeros
	void *aux = calloc(res, 1);
	fwrite(aux, 1, res, fp);
	free(aux);
}

// libera memoria do registro Escola alocado
void free_escola(Escola* r){
	free(r->dataInicio);
	free(r->dataFinal);
	free(r->nomeEscola);
	free(r->municipio);
	free(r->endereco);
	free(r);
}

// Retorna um registro Escola a partir da posicao do arquivo apontada pelo fp. Transforma os campos de caracteres em strings novamente para facilitar a impressao
Escola *le_bin_escola(FILE *fp){
	int tamReg1, tamReg2, res;

	tamReg1 = ftell(fp);

	Escola* r = calloc(1,sizeof(Escola)); 

	fread(&r->codEscola, sizeof(int), 1, fp);

	r->dataInicio = malloc(sizeof(char) * (TAM_CAMPO + 1));
	fread(r->dataInicio, sizeof(char), TAM_CAMPO, fp);
	r->dataInicio[TAM_CAMPO] = '\0';

	r->dataFinal = malloc(sizeof(char) * (TAM_CAMPO + 1));
	fread(r->dataFinal, sizeof(char), TAM_CAMPO, fp);
	r->dataFinal[TAM_CAMPO] = '\0';

	fread(&r->tamEscola, sizeof(int), 1, fp);
	if(r->tamEscola != 0){
		r->nomeEscola = malloc(sizeof(char) * (r->tamEscola + 1));
		fread(r->nomeEscola, sizeof(char), r->tamEscola, fp);
		r->nomeEscola[r->tamEscola] = '\0';
	}

	fread(&r->tamMunicipio, sizeof(int), 1, fp);
	if(r->tamMunicipio != 0){
		r->municipio = malloc(sizeof(char) * (r->tamMunicipio + 1));
		fread(r->municipio, sizeof(char), r->tamMunicipio, fp);
		r->municipio[r->tamMunicipio] = '\0';
	}

	fread(&r->tamEndereco, sizeof(int), 1, fp);
	if(r->tamEndereco != 0){
		r->endereco = malloc(sizeof(char) * (r->tamEndereco + 1));
		fread(r->endereco, sizeof(char), r->tamEndereco, fp);
		r->endereco[r->tamEndereco] = '\0';
	}

	tamReg2 = ftell(fp);
	res = TAM_REG - (tamReg2 - tamReg1);
	// pula os bytes vazios
	fseek(fp, res, SEEK_CUR);
	return r;
}

// Função 9: recupera pilha
void recupera_pilha(char *nomeArquivo){
	//funcao printa os valores do topo da pilha enquando esses existirem (forem diferentes de -1)

	FILE *fp = fopen(nomeArquivo,"rb+");
	if(!verifica_status(fp))
		return;

	int offset;
	int topo = topoPilha(fp);
	if(topo == -1){ //a pilha estava vazia, não tem o que printar 
		printf("Pilha vazia.");
	}
	else{
		while(topo != -1){ //enquanto o valor do topo for diferente de -1, printo o valor do topo, ando com o ponteiro de arquivo, para
			//o local indicado por topo, e leio o novo valor de topo
			printf("%d ", topo);
			offset = (topo * TAM_REG) + TAM_CABECALHO;
			fseek(fp, offset + sizeof(int), SEEK_SET);
			fread(&topo, sizeof(int), 1, fp);
		}
	}
	printf("\n");
	fclose(fp);
}

// Função 8: compacta o arquivo de forma eficiente
void desfragmenta(char* nomeArquivo){
	Escola *r;
	//funcionalidade cria um novo arquivo , copiando o antigo sem os registros removidos, assim desfragmenta o arquivo de maneira inteligente
	FILE *fp,*fn;
	fp = fopen(nomeArquivo,"rb+");

	if(!verifica_status(fp))
		return;

	rename(nomeArquivo, "antigo.bin");

	fn = fopen(nomeArquivo, "wb+"); //criando um arquivo novo, que sera o arquivo desfragmentado
	criaCabecalho(fn); // cria o cabeçalho, status 0 (inconsistente) e topoPilha -1. O byte offset ja esta no fim do arquivo, pode começar a inserir

	fseek(fp, 0, SEEK_END);
	int	tam = ftell(fp);

	if(tam > TAM_CABECALHO){
		fseek(fp, TAM_CABECALHO, SEEK_SET);
		while(ftell(fp) < tam){
			if(existencia_registro(fp)){
				r = le_bin_escola(fp); //le o registro válido do arquivo antigo
				escreve_bin_escola(fn,r); //escreve apenas os registros validos no arquivo novo
				free_escola(r);
			}else { //pulando o registro invalido
				fseek(fp, TAM_REG, SEEK_CUR);
			}
		}
	}

	fclose(fp);
	remove("antigo.bin"); //apagando o arquivo antigo
	printf("Arquivo de dados compactado com sucesso.\n");

	atualizaStatus(fn, 1);
	fclose(fn);
}

// Função 7: atualiza registro
void atualiza_registro(char *nomeArquivo, char** argv){
	Escola *r;
	char* vazio = "0000000000";

	FILE *fp = fopen(nomeArquivo,"rb+");
	if(!verifica_status(fp))
		return;
	atualizaStatus(fp, 0); //atualiza o status do arquivo como insconsistente

	int RRN, offset; 
	RRN = atoi(argv[2]);
	offset = (RRN * TAM_REG) + TAM_CABECALHO; //calculando o byte offset que corresponde a posição do registro a ser alterada

	fseek(fp, 0, SEEK_END);
	int tam = ftell(fp);
	if(offset >= tam || RRN < 0) { //verificando se o byte offset esta no arquivo e se o RRN é válido
		printf("Registro inexistente.\n");
		return;
	}

	fseek(fp, offset, SEEK_SET);
	if(!existencia_registro(fp)){ //se o registro nao for válido, ja encerra a função aqui
		printf("Registro inexistente.\n");
		return;
	}

	r = calloc(1, sizeof(Escola));
	r->codEscola = atoi(argv[3]);

	if(strlen(argv[4]) != 0){ //checando se a entrada por linha de comando nao é vazia, se nao for, atualiza o registro
		r->dataInicio = argv[4];
	}
	else{ //caso o contrário, colocando o campo como vazio
		r->dataInicio = strdup(vazio);
	}

	if(strlen(argv[5]) != 0){ //checando se a entrada por linha de comando nao é vazia, se nao for, atualiza o registro
		r->dataFinal = argv[5];
	}
	else{ //caso o contrário, colocando o campo como vazio
		r->dataFinal = strdup(vazio);
	}

	if(strlen(argv[6]) != 0){ //checando se a entrada por linha de comando nao é vazia, se nao for, atualiza o registro
		r->nomeEscola = argv[6];
		r->tamEscola = strlen(argv[6]);
	}
	else{  //caso o contrário, colocando o campo como vazio
		r->tamEscola = 0;
	}

	if(strlen(argv[7]) != 0){ //checando se a entrada por linha de comando nao é vazia, se nao for, atualiza o registro
		r->municipio = argv[7];
		r->tamMunicipio = strlen(argv[7]);
	}
	else{  //caso o contrário, colocando o campo como vazio
		r->tamMunicipio = 0;
	}

	if(strlen(argv[8]) != 0){ //checando se a entrada por linha de comando nao é vazia, se nao for, atualiza o registro
		r->endereco = argv[8];
		r->tamEndereco = strlen(argv[8]);
	}
	else{   //caso o contrário, colocando o campo como vazio
		r->tamEndereco = 0;
	}
	escreve_bin_escola(fp, r);

	free(r->dataInicio);
	free(r->dataFinal);
	free(r);

	printf("Registro alterado com sucesso.\n");

	atualizaStatus(fp, 1); //atualiza o status do arquivo como consistente
	fclose(fp);
}

// Função 6: insere registro no arquivo de dados binario e no indice de arvore B
void insere_registro(char* nomeArquivo, char** argv){
	Escola *r;
	int topo, offset, RRN_dados;
	char *vazio = "0000000000";

	FILE *fp = fopen(nomeArquivo,"rb+");
	FILE *fb = fopen("indice.bin", "rb+");
	BufferPool bp;
	iniciaBufferPool(&bp);

	if(!verifica_status(fp) || !verifica_status_B(fb)){
		return;
	}
	atualizaStatus(fp, 0); //atualiza o status do arquivo como insconsistente
	atualiza_status_B(fb, 0); // atualiza o arquivo de indice como inconsistente

	topo = topoPilha(fp); //recuperando o topo da pilha
	if(topo == -1){ //se o topo for -1, nao existe nenhum elemento removido no arquivo, logo devo inserir no final
		fseek(fp, 0, SEEK_END);
		offset = ftell(fp);
		RRN_dados = (offset - TAM_CABECALHO)/TAM_REG; // definindo RRN para o arquivo de indice
	}
	else{
		offset = (topo * TAM_REG) + TAM_CABECALHO; //calculo o byte offset onde iremos inserir o novo registro
		fseek(fp, offset, SEEK_SET);
		fseek(fp, sizeof(int), SEEK_CUR); // pulando os 4 bytes que indicam arquivo removido (-1)
		fread(&topo, sizeof(int), 1, fp); //lendo o novo topo da pilha
		fseek(fp, -2*sizeof(int), SEEK_CUR);//voltando para o inicio do registro (byte offset)
		RRN_dados = topo; // definindo RRN para o arquivo de indice
	}

	r = calloc(1, sizeof(Escola));

	r->codEscola = atoi(argv[2]);

	if(atoi(argv[3]) != 0){  //inserindo o novo campo
		r->dataInicio = argv[3];
	}
	else{  //insere o campo vazio
		r->dataInicio = strdup(vazio);
	}

	if(atoi(argv[4]) != 0){ //inserindo o novo campo
		r->dataFinal = argv[4];
	}
	else{ //insere o campo vazio
		r->dataFinal = strdup(vazio);
	}

	if(strlen(argv[5]) != 0){ //inserindo o novo campo
		r->nomeEscola = argv[5];
		r->tamEscola = strlen(argv[5]);
	}
	else{ //insere o tam do campo como 0, demonstrando que o campo esta vazio
		r->tamEscola = 0;
	}

	if(strlen(argv[6]) != 0){ //inserindo o novo campo
		r->municipio = argv[6];
		r->tamMunicipio = strlen(argv[6]);
	}
	else{ //insere o tam do campo como 0, demonstrando que o campo esta vazio
		r->tamMunicipio = 0;
	}

	if(strlen(argv[7]) != 0){ //inserindo o novo campo
		r->endereco = argv[7];
		r->tamEndereco = strlen(argv[7]);
	}
	else{ //insere o tam do campo como 0, demonstrando que o campo esta vazio
		r->tamEndereco = 0;
	}
	escreve_bin_escola(fp, r); //escrevendo o registro no arquivo
	Btree_Insert(fb, r->codEscola, RRN_dados, &bp);
	free(r->dataInicio);
	free(r->dataFinal);
	free(r);

	insere_pilha(topo, fp); //atualizabdo o topo da pilha

	printf("Registro inserido com sucesso.\n");

	flush(fb, &bp);
	atualizaStatus(fp, 1); //atualiza o status do arquivo como consistente
	atualiza_status_B(fb, 1); // atualiza o arquivo de indice como consistente
	fclose(fp);
	fclose(fb);
}

// Função 5: Remove logicamente um registro, colocando '-1' em seus primeiros 4 bytes (int). Seguido pela pilha, atualizando-a
void remocao_logica(char *nomeArquivo, int RRN){
	FILE *fp = fopen(nomeArquivo,"rb+");
	if(!verifica_status(fp))
		return;
	atualizaStatus(fp, 0); //atualiza o status do arquivo como insconsistente

	int topo = topoPilha(fp); //salvando o RRN do ultimo registro removido
	int offset =  (RRN * TAM_REG) + TAM_CABECALHO; //calculando o byte offset que o registro a ser removido se encontra

	fseek(fp, 0, SEEK_END);
	int tam = ftell(fp);

	if(offset >= tam || RRN < 0) { //checando se o byte offset esta no arquivo, e o RRN contem um valor válido
		printf("Registro inexistente.\n");
	}else {
		fseek(fp, offset, SEEK_SET);
		if(existencia_registro(fp)){ //ve se o arquivo é válido

			int rm = -1;
			fwrite(&rm, sizeof(int), 1, fp); //marcando os primeiros 4 bytes como -1, sinalizando que o registro esta removido
			fwrite(&topo, sizeof(int), 1, fp); //nos proximos 4 bytes, armazena o antigo topo da pilha
			insere_pilha(RRN,fp); //atauliza o topo da pilha

			printf("Registro removido com sucesso.\n");
		}
		else { //registro a ser removido nao foi encontrado
			printf("Registro inexistente.\n");
		}
	}

	atualizaStatus(fp, 1); //atualiza o status do arquivo como consistente
	fclose(fp);
}

// Função 4: recupera um registro do arquivo de dados pelo RRN
void recupera_reg(char *nomeArquivo, int RRN){
	Escola* r;

	FILE *fp = fopen(nomeArquivo,"rb+");
	if(!verifica_status(fp))
		return;

	int offset =  (RRN * TAM_REG) + TAM_CABECALHO; //calculando o byte offset que o registro buscado se encontra

	fseek(fp, 0, SEEK_END);
	int tam = ftell(fp);

	if(offset >= tam || RRN < 0) { //checando se o arquivo possui o offset e se o RRN é válido
		printf("Registro inexistente.\n");
	}else {
		fseek(fp, offset, SEEK_SET);// movendo o ponteiro fp para a posição que desejo ler.
		if(existencia_registro(fp)){ //checo se o registro é valido, leio o registro e imprimo o seu conteudo
			r = le_bin_escola(fp);
			print_escola(r);
			free_escola(r);
		}
		else{ //registro invalido (registro removido)
			printf("Registro inexistente.\n");
		}
	}
	fclose(fp);
}

// Função 3: Pesquisa por campo
void pesquisa_campo(char *nomeArquivo, char** argv){

	Escola* r;
	char* campo, *valor;
	campo = argv[2]; //recebendo a entrada por linha de comando de qual campo estarei buscando o valor
	valor = argv[3]; //qual valor estarei buscando
	int tam, aux, count;

	FILE *fp = fopen(nomeArquivo,"rb+");
	if(!verifica_status(fp))
		return;

	fseek(fp, 0, SEEK_END);
	tam = ftell(fp);
	fseek(fp, TAM_CABECALHO, SEEK_SET);
	count = 0;
	while(ftell(fp) < tam){ //enquanto nao cheguei no final do arquivo, checo o registro
		if(existencia_registro(fp)){
			//checo se o registro é válido (nao foi removido)
			r = le_bin_escola(fp);
			if(!strcmp(campo, "codEscola")){ //checa se o campo buscado é codEscola
				aux = atoi(valor);
				if(r->codEscola == aux){ //checa se o valor do campo é o mesmo do valor buscado
					print_escola(r);
					count++;
				}
			}
			else if(!strcmp(campo, "dataInicio")){ //checa se o campo buscado é dataInicio
				if(!strcmp(r->dataInicio, valor)){ //checa se o valor do campo é o mesmo do valor buscado
					print_escola(r);
					count++;
				}
			}
			else if(!strcmp(campo, "dataFinal")){ //checa se o campo buscado é dataFinal
				if(!strcmp(r->dataFinal, valor)){ //checa se o valor do campo é o mesmo do valor buscado
					print_escola(r);
					count++;
				}
			}
			else if(!strcmp(campo, "nomeEscola")){ //checa se o campo buscado é nomeEscola
				if(r->tamEscola != 0){ //checa se esse campo nao esta vazio
					if(!strcmp(r->nomeEscola, valor)){ //checa se o valor do campo é o mesmo do valor buscado
						print_escola(r);
						count++;
					}
				}
			}
			else if(!strcmp(campo, "municipio")){ //checa se o campo buscado é muninipio
				if(r->tamMunicipio != 0){ //checa se esse campo nao esta vazio
					if(!strcmp(r->municipio, valor)){ //checa se  o valor do campo é o mesmo do valor buscado
						print_escola(r);
						count++;
					}
				}
			}
			else if(!strcmp(campo, "endereco")){ //checa se o campo buscado é endereço
				if(r->tamEndereco != 0){//checa que esse campo nao esta vazio
					if(!strcmp(r->endereco, valor)){ //checa se  o valor do campo é o mesmo do valor buscado
						print_escola(r);
						count++;
					}
				}
			}
			free_escola(r);
		}else { //pulando o registro inválido 
			fseek(fp, TAM_REG, SEEK_CUR);
		}
	}
	if(count == 0){ //nao existe nenhum registro que possui em seu campo o valor buscado
		printf("Registro inexistente.\n");
	}

	fclose(fp);
}

// Função 2: Recuperação de dados de todos os registros
void recupera_dados(char* nomeArquivo){
	Escola* r;
	int tam,encontrouReg = 0;

	FILE *fp = fopen(nomeArquivo,"rb+");
	if(!verifica_status(fp))
		return;

	fseek(fp, 0, SEEK_END);
	tam = ftell(fp);
	if(tam == TAM_CABECALHO){ //se o tamanho do arquivo for igual ao tamanho do cabeçalho do arquivo, quer dizer que nao existem nenhum registro a ser recuperado
		printf("Registro inexistente.\n");
	}
	else{
		fseek(fp, TAM_CABECALHO, SEEK_SET);
		while(ftell(fp) < tam){ //enquanto nao andei todos os registros (cheguei no final do arquivo)
			if(existencia_registro(fp)){ //checo se o registro que desejo ler é valido, isto é, ele nao foi removido ainda
				r = le_bin_escola(fp);
				print_escola(r);
				free_escola(r); //libera o espaço de memoria do registro temporario
				encontrouReg = 1;//setando uma flag como true, mostrando que ao menos um registro foi resgatado
			}else { //pulando o arquivo que nao pode ser lido (removido)
				fseek(fp, TAM_REG, SEEK_CUR);
			}
		}

	}
	if(encontrouReg == 0) { //se a flag encontrouReg estiver false, nenhum registro foi resgatado
		printf("Registro inexistente.\n");
	}
	fclose(fp);
}

// Função 1: le um arquivo de entrada e escreve um binario
void le_csvQ(char *nomeEntrada, char *nomeSaida){
	char *linha;
	Escola *r;
	FILE *fe, *fs;
	FILE *fb;
	int offset = -1;
	
	if((fe = fopen(nomeEntrada, "r")) == NULL) { // verifica se encontrou um arquivo de entrada
		printf("Falha no carregamento do arquivo.\n");
		return;
	}

	fs = fopen(nomeSaida, "wb+");
	fb = fopen("indice.bin", "wb+");
	criaCabecalho(fs); // cria o cabeçalho, status 0 (inconsistente) e topoPilha -1. O byte offset ja esta no fim do arquivo, pode começar a inserir
	cria_Cabecalho_B(fb);//cria o cabeçalho do indice arvore b, status 0 inconsistente
	BufferPool bp;
	iniciaBufferPool(&bp);

	for(int i=0;i<5;i++){
		printf("%d ",bp.RRN[i]);
	}
	printf("\n");

	while(fscanf(fe, "%m[^\r\n]%*[\r\n]", &linha) != EOF){
		r = parse_linha(linha); //separa os campos de um registro a partir do.CSV de entrada
		escreve_bin_escola(fs, r); 
		Btree_Insert(fb, r->codEscola, ++offset, &bp);
		free_escola(r); //libera o espaço de memoria do registro temporario
		free(linha);
	}

	flush(fb, &bp);	
	fclose(fe);
	atualizaStatus(fs, 1); // define o arquivo de dados como consistente
	atualiza_status_B(fb, 1); // define o arquivo de indice como consistente
	fclose(fs);
	fclose(fb);
	printf("Arquivo carregado.\n");
}


int main(int argc, char *argv[]){
	char *nomeEntrada, *nomeSaida = "saida.bin";
	if(argc == 1) {
		printf("Falha no processamento do arquivo.\n");
		return 0;
	}

	int func = atoi(argv[1]);

	if(func > 15 || func < 1){
		printf("Operacao \"%d\" nao reconhecida\n", func);
		return 0;
	}

	if(func == 1){ // le o arquivo de entrada (csv) e cria o arquivo de dados
		nomeEntrada = argv[2];
		le_csvQ(nomeEntrada, nomeSaida);
	}
	else if(func == 2){
		recupera_dados(nomeSaida);
	}
	else if(func == 3){
		pesquisa_campo(nomeSaida, argv);
	}
	else if(func == 4){
		int RRN = atoi(argv[2]);
		recupera_reg(nomeSaida,RRN);
	}
	else if(func == 5){
		int RRN = atoi(argv[2]);
		remocao_logica(nomeSaida, RRN);
	}
	else if(func == 6){
		insere_registro(nomeSaida, argv);
	}
	else if(func == 7){
		atualiza_registro(nomeSaida, argv);
	}
	else if(func == 8){
		desfragmenta(nomeSaida);
	}
	else if(func == 9){
		recupera_pilha(nomeSaida);
	}
	else if(func == 10){
		nomeEntrada = argv[2];
		le_csvQ(nomeEntrada, nomeSaida);
		//imprime_indice();
	}
	else if(func == 11){
		insere_registro(nomeSaida, argv);
	}
	else if(func == 12){
		int chave  = atoi(argv[2]);
		busca(chave);
	}
	return 0;
}
