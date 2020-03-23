/*
 * Copyright (c) 1991-2016 Kawahara Lab., Kyoto University
 * Copyright (c) 2000-2005 Shikano Lab., Nara Institute of Science and Technology
 * Copyright (c) 2005-2016 Julius project team, Nagoya Institute of Technology
 * All rights reserved
 */

/* include top Julius library header */
#include <julius/juliuslib.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>

#include "jtalk.h"

/* 音素リスト */
typedef enum {head,body,tail} pos;

//入力されたメッセージを合成音声して、出力する
void say(OpenJTalk *oj, char *message)
{
  /*同期発声*/
  openjtalk_speakSync(oj,message); 
}

//音声認識準備が整ったら呼び出されるコールバック
static void
status_recready(Recog *recog, void *dummy)
{
  if (recog->jconf->input.speech_input == SP_MIC || recog->jconf->input.speech_input == SP_NETAUDIO) {
    fprintf(stderr, "\r        待機中            \r");
  }
}


//音声入力がトリガーされたときに呼び出されるコールバック
static void
status_recstart(Recog *recog, void *dummy)
{
  if (recog->jconf->input.speech_input == SP_MIC || recog->jconf->input.speech_input == SP_NETAUDIO) {
    fprintf(stderr, "\r     認識中   \r");
  }
}

//音素列を出力する副機能
static void
put_hypo_phoneme(WORD_ID *seq, int n, WORD_INFO *winfo, Sentence *s, OpenJTalk *oj)
{
  int f=0,g=0,h,i,j,k,l;
  int count = 0,count1 = 0,count2 = 0; //カウント用変数
  int flag1 = 0, flag2 = 0;     //フラグ用変数
  char str[256];
  WORD_ID w;
  pos p;
  int key[MAX_HMMNAME_LEN];
  int key1[MAX_HMMNAME_LEN];
  int key2[MAX_HMMNAME_LEN];
  int key3[MAX_HMMNAME_LEN];
  static char phoneme[2];
  static char buf[MAX_HMMNAME_LEN];
  static char buf1[MAX_HMMNAME_LEN];
  static char buf2[MAX_HMMNAME_LEN];
  FILE *fp; //ファイル構造体へのポインタを宣言

  /* 構造体 struct phlistの宣言 */
  struct ph_list {

        char ph[3];
        pos  p;

  } List[40]; //日本語の場合、音素はローマ字の1文字を単位として考えると20種類
              //長音や拗音を区別すると40種類

  /* 音素リストを設定したテキストファイルを読み込む */
  /* 読み込みモードでファイルを開く */
  if((fp = fopen("list.txt","r")) == NULL){
      fprintf(stderr, "%s\n","error:can't read file");
      printf("音素リストの読み込みに失敗しました");
    }

  //ファイルの終端(EOF)になるまで続ける
  //ファイルから文字を読み込みList.phに格納
  while(fscanf(fp,"%s",str) != EOF){
      
      if(f%2 == 0){
            strcpy(List[count2].ph,str);
          count2++;
           }

      else {
        if(strcmp(str,"語頭") == 0)
            List[count2-1].p = head;

        else if(strcmp(str,"語中") == 0)
            List[count2-1].p = body;

        else if(strcmp(str,"語尾") == 0)
            List[count2-1].p = tail;
        else
            printf("音素リストの設定が間違っています\n");
        }
        f++;
      }
  //ファイルを閉じる
  fclose(fp);

  /* 初期化処理 */
  for(h=0;h<n;h++){
    key[h] = 0;
    key1[h] = 0;
    key2[h] = 0;
    key3[h] = 0;
  }

  if (seq != NULL) {
    for (i=0;i<n;i++) {
      w = seq[i];
      for (j=0;j<winfo->wlen[w];j++) {

          if(j == 0) p = head;  //語頭
          else if(j == (winfo->wlen[w]) - 1) p = tail; //語尾
          else p = body;       //語中

          	/* 音素の抽出 */
        	center_name(winfo->wseq[w][j]->name, buf);
        
          for(h=0;h<2;h++) phoneme[h] = buf[h]; 

           /* 音素が１文字の場合
            * 音素１文字は　a_ N_ などが該当
            * a_ → a にする
            */
	      if(phoneme[1] == '_') 
	           phoneme[1] = '\0';

	       /* 音素が2文字の場合
	          音素2文字は o: sh u; などが該当
	        */
	      else {
	           key2[j] = 1;
	           count1++;
	          }
          /* buf1に認識した音素を連結していく */
          strcat(buf1,phoneme);

          switch(p){
            /* 先頭の音素 */
            case head:
              for(f=0; f<count2; f++){
                  if((strcmp(phoneme,List[f].ph) == 0)
                      && List[f].p == head){
                    flag1 = 1; 
                    flag2 = 1;
                    key1[j] = 1;
                  }
                }
              break;
            /* 語中の音素 */
            case body:
              for(f=0; f<count2; f++){
                if((strcmp(phoneme,List[f].ph) == 0)
                     && List[f].p == body) {
                  flag1 = 1; 
                  flag2 = 1;
                  key1[j] = 1;
                }
              }
              break;
            /* 語尾の音素 */
            case tail:
              for(f=0; f<count2; f++){
                if((strcmp(phoneme,List[f].ph) == 0)
                    && List[f].p == tail) {
                  flag1 = 1;
                  flag2 = 1;
                  key1[j] = 1;
                }
              }
              break;

            default:
	            break;
          }
        }

        if(flag1 == 1){

          /* 抜き出された単語の単語信頼度が70以上 かつ その単語が名詞の場合*/
          if( (100*(s->confidence[i]) >= 70 
            && (strstr(winfo->wname[seq[i]],"名詞")) != NULL )) {

          	/* 検出された単語を表示 */
            printf("Detected word:");
            printf(" \x1b[31m%s\033[m\n", winfo->woutput[seq[i]]);

            /* 検出された音素を表示 */
            printf("Detected phone:");

            /* 音素リストにマッチした箇所を赤く表示、それ以外は黒く表示*/
            for(h=0; h < winfo->wlen[w] + count1; h++){

              if(key1[g] == 1){

              	  if(key2[g] == 1) {
                 		printf("\x1b[31m%c%c \033[m",buf1[h],buf1[h+1]);
                 		h++;
                 		}
             	  else 
             			printf("\x1b[31m%c \033[m",buf1[h]);
             		}

              else {
              	  	if(key2[g] == 1){
                		printf("%c%c ",buf1[h],buf1[h+1]);
                		h++;
              	  	}
            	  	else
            	  		printf("%c ",buf1[h]);
            	   }
           	   g++;
            }
            printf("\n");

            /* 単語信頼度を表示 */
            printf("cmscore：");
            printf(" %3.1f%%\n", 100 * (s->confidence[i]));

            /* 検出された単語を合成音声で出力 */
            say(oj,winfo->woutput[seq[i]]);

            count++;

            key[i] = 1;
          } 
       }
        /* 単語信頼度が最低閾値：20を下回る場合 */
        if(100*(s->confidence[i]) < 20) key3[i] = 1;

        flag1 = 0;
        count1 = 0;
        g = 0;

        for(h=0;h<winfo->wlen[w];h++) {
          buf1[h] = '\0';
          key1[h] = 0;
          key2[h] = 0;
        }
    }
    /*  キーワードとして抜き出された全ての単語の単語信頼度が低かった場合 */
    if(flag2 == 1 && count == 0)
        printf("もう一度言ってください\n");

    count = 0;
    flag2 = 0;

    /* 認識結果全文表示 */
  printf("音声認識結果:");
  for(k=0;k<n;k++) {
    //キーワードを赤く表示
    if(key[k] == 1) printf(" \x1b[31m%s\033[m", winfo->woutput[seq[k]]);
    //最低閾値を下回った単語を灰色で表示
    else if(key3[k] == 1) printf(" \x1b[37m%s\033[m", winfo->woutput[seq[k]]);

    else  printf(" %s", winfo->woutput[seq[k]]);

        }
  printf("\n"); 
  }
}


//最終認識結果を出力するためのコールバック。
//この関数は入力の認識が終了した直後に呼ばれる
static void
output_result(Recog *recog, void *dummy)
{
  int i, j;
  int len;
  WORD_INFO *winfo;
  WORD_ID *seq;
  int seqnum;
  int n;
  Sentence *s;
  RecogProcess *r;
  HMM_Logical *p;
  SentenceAlign *align;
  OpenJTalk *oj;


  /* 音響モデルファイル、辞書フォルダ、音響モデルフォルダを設定する*/
  oj = openjtalk_initialize
  ("nitech_jp_atr503_m001.htsvoice","/usr/local/Cellar/open-jtalk/1.10_1/dic","/Users/tsukasa1260/Sound/Julius/keyword_spotting_c/jtalkdll/voice/hts_voice_nitech_jp_atr503_m001-1.05");

  /* サンプリング周波数を設定 */
  openjtalk_set_s(oj,48000);

  /* フレームピリオドを設定 */
  openjtalk_set_p(oj,240);

  /* オールパス値 */
  openjtalk_set_a(oj,0.55);

  /* ポストフィルター係数 */
  openjtalk_set_b(oj,0.0);

   /* 発声速度を設定 */
  openjtalk_set_r(oj,0.4);

  /* 追加ハーフトーン */
  openjtalk_set_fm(oj,0.0);

  /* 有声/無声境界値 */
  openjtalk_set_u(oj,0.5);

  /* スペクトラム系列内変動の重み */
  openjtalk_set_jm(oj,1.0);

  /* F0系列内変動重み */
  openjtalk_set_jf(oj,1.0);

  /* 発声音量を設定 */
  openjtalk_set_g(oj,5.0);



  //すべての認識結果は各認識プロセスで保存
  for(r=recog->process_list;r;r=r->next) {

    //プロセスが稼働していない場合は、プロセスをスキップ
    if (! r->live) continue;


    /* check result status */
    if (r->result.status < 0) {      //認識結果が得られない時

      //ステータスコードに従ってメッセージを出力
      switch(r->result.status) {
      case J_RESULT_STATUS_REJECT_POWER:
	printf("入力が電源によって拒否されました\n");
	break;
      case J_RESULT_STATUS_TERMINATE:
	printf("入力が要求によって終了しました\n");
	break;
      case J_RESULT_STATUS_ONLY_SILENCE:
	printf("デコーダによって入力が拒否されました（無音入力結果です)\n");
	break;
      case J_RESULT_STATUS_REJECT_GMM:
	printf("GMMによって入力が拒否されました\n");
	break;
      case J_RESULT_STATUS_REJECT_SHORT:
	printf("入力された音声が短いです\n");
	break;
      case J_RESULT_STATUS_REJECT_LONG:
	printf("入力された音声が長すぎます\n");
	break;
      case J_RESULT_STATUS_FAIL:
	printf("検索失敗しました\n");
	break;
      }
      //次のプロセスに進む
      continue;
    }

    //得られた全文の出力結果
    winfo = r->lm->winfo;

    for(n = 0; n < r->result.sentnum; n++) { //全てのセンテンスを処理するまでループ

      s = &(r->result.sent[n]);
      seq = s->word;
      seqnum = s->word_num;


      /* phoneme sequence */
      //printf("phseq%d:", n+1);
      put_hypo_phoneme(seq, seqnum, winfo,s,oj);
      printf("\n");

      if (r->lmtype == LM_DFA) { /* if this process uses DFA grammar */
	/* output which grammar the hypothesis belongs to
	   when using multiple grammars */
	if (multigram_get_all_num(r->lm) > 1) {
	  printf("grammar%d: %d\n", n+1, s->gram_id);
	}
      }
      
      //整列結果を出力
      for (align = s->align; align; align = align->next) {
	printf("=== begin forced alignment ===\n");
	switch(align->unittype) {
	case PER_WORD:
	  printf("-- word alignment --\n"); break;
	case PER_PHONEME:
	  printf("-- phoneme alignment --\n"); break;
	case PER_STATE:
	  printf("-- state alignment --\n"); break;
	}
	printf(" id: from  to    n_score    unit\n");
	printf(" ----------------------------------------\n");
	for(i=0;i<align->num;i++) {
	  printf("[%4d %4d]  %f  ", align->begin_frame[i], align->end_frame[i], align->avgscore[i]);
	  switch(align->unittype) {
	  case PER_WORD:
	    printf("%s\t[%s]\n", winfo->wname[align->w[i]], winfo->woutput[align->w[i]]);
	    break;
	  case PER_PHONEME:
	    p = align->ph[i];
	    if (p->is_pseudo) {
	      printf("{%s}\n", p->name);
	    } else if (strmatch(p->name, p->body.defined->name)) {
	      printf("%s\n", p->name);
	    } else {
	      printf("%s[%s]\n", p->name, p->body.defined->name);
	    }
	    break;
	  case PER_STATE:
	    p = align->ph[i];
	    if (p->is_pseudo) {
	      printf("{%s}", p->name);
	    } else if (strmatch(p->name, p->body.defined->name)) {
	      printf("%s", p->name);
	    } else {
	      printf("%s[%s]", p->name, p->body.defined->name);
	    }
	    if (r->am->hmminfo->multipath) {
	      if (align->is_iwsp[i]) {
		printf(" #%d (sp)\n", align->loc[i]);
	      } else {
		printf(" #%d\n", align->loc[i]);
	      }
	    } else {
	      printf(" #%d\n", align->loc[i]);
	    }
	    break;
	  }
	}
	
	printf("re-computed AM score: %f\n", align->allscore);

	printf("=== end forced alignment ===\n");
      }
    }
  }

  /* flush output buffer */
  fflush(stdout);
}


int
main(int argc, char *argv[])
{
  Jconf *jconf; // 設定パラメータの格納変数
  Recog *recog; //// 認識コアのインスタンス変数
  int ret,check;

  static char speechfilename[MAXPATHLEN];  //MFCCファイル入力用の音声ファイル名


  //ログ機能をOFF
  jlog_set_output(NULL);

  //-Cオプションで指定するjconfファイルは必要なので確認
  if (argc == 1) {
    fprintf(stderr, "Julius rev.%s - based on ", JULIUS_VERSION);
    j_put_version(stderr);
    fprintf(stderr, "Try '-setting' for built-in engine configuration.\n");
    return -1;
  }

  //指定したjconfファイルから設定を読み込む
  jconf = j_config_load_args_new(argc, argv);

  if (jconf == NULL) {
    fprintf(stderr, "Try '-setting' for built-in engine configuration.\n");
    return -1;
  }
  
  //読み込んだjconfファイルから認識器を作成
  recog = j_create_instance_from_jconf(jconf);

  if (recog == NULL) {
    fprintf(stderr, "Error in startup\n");
    return -1;
  }

  //各種コールバックの作成
  
  //音声認識準備が整ったらstatus_recreadyを呼び出す
  callback_add(recog, CALLBACK_EVENT_SPEECH_READY, status_recready, NULL);

  //音声認識処理を行っているときにstatus_recstartを呼び出す
  callback_add(recog, CALLBACK_EVENT_SPEECH_START, status_recstart, NULL);

  //output_resultを呼び出し、認識結果を出力
  callback_add(recog, CALLBACK_RESULT, output_result, NULL);

  //音声入力を初期化
  if (j_adin_init(recog) == FALSE) {    /* error */
    return -1;
  }

  //入力デバイスの確認
  
  if (jconf->input.speech_input == SP_MFCFILE || jconf->input.speech_input == SP_OUTPROBFILE) {
    /* ファイル入力 */

    while (get_line_from_stdin(speechfilename, MAXPATHLEN, "enter MFCC filename->") != NULL) {
      if (verbose_flag) printf("\ninput MFCC file: %s\n", speechfilename);
      /* open the input file */
      ret = j_open_stream(recog, speechfilename);
      switch(ret) {
      case 0:			/* succeeded */
	      break;
      case -1:      		/* error */
	      continue;

      case -2:			/* end of recognition */
	      return 0;
      }

      //入力から認識を行うループ
      ret = j_recognize_stream(recog);

      //内部でエラーが発生 
      //retの戻り値が0の場合は正常終了
      if (ret == -1) return -1;	
    }

  } else {
    /* マイクなどによる音声入力 */

    switch(j_open_stream(recog, NULL)) {
    case 0:			/* succeeded */
      break;
    case -1:      		/* error */
      fprintf(stderr, "error in input stream\n");
      return 0;
    case -2:			/* end of recognition process */
      fprintf(stderr, "failed to begin input stream\n");
      return 0;
    }
    
    ///入力から認識を行うループ
    ret = j_recognize_stream(recog);

    //内部でエラーが発生 
    //retの戻り値が0の場合は正常終了
    if (ret == -1) return -1;
    
  }

  //終了処理
    
  //ストリームを閉じる
  j_close_stream(recog);
  //インスタンスを解放する
  j_recog_free(recog);

  return(0);
}
