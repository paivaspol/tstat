/*
 *
 * Copyright (c) 2001-2008
 *	Politecnico di Torino.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For bug report and other information please visit Tstat site:
 * http://tstat.polito.it
 *
 * Tstat is deeply based on TCPTRACE. The authors would like to thank
 * Shawn Ostermann for the development of TCPTRACE.
 *
 */

#include "tstat.h"
#include "http.h"
#include "tcpL7.h"
#include <regex.h>

#define HTTP_SMALL_BUFFER_SIZE  200
#define HTTP_LARGE_BUFFER_SIZE 1600

char *http_patterns[14];
regex_t http_re[14];
regmatch_t re_res[3];
char http_url[HTTP_LARGE_BUFFER_SIZE];
char http_url_private[HTTP_LARGE_BUFFER_SIZE];
char http_method[10];
char http_host[HTTP_SMALL_BUFFER_SIZE];
char http_ua[HTTP_LARGE_BUFFER_SIZE];
char http_ctype[HTTP_SMALL_BUFFER_SIZE];
char http_clen[HTTP_SMALL_BUFFER_SIZE];
char http_referer[HTTP_LARGE_BUFFER_SIZE];
char http_referer_private[HTTP_LARGE_BUFFER_SIZE];
char http_response[5];
char http_range[HTTP_SMALL_BUFFER_SIZE];
char http_server[HTTP_SMALL_BUFFER_SIZE];

static char truncated_field[] = "[*]";
static char full_field[] = "";

extern FILE *fp_http_logc;
extern u_int32_t http_full_url;

void init_http_patterns()
{
  int i;
  
  http_patterns[0] = "^([A-Z]+) ([^[:cntrl:][:space:]]+) ";
  http_patterns[1] = "Host: ([^[:cntrl:][:space:]]+)";
  http_patterns[2] = "Referer: ([^[:cntrl:][:space:]]+)";
  http_patterns[3] = "User-Agent: ([^[:cntrl:]]+)";
  http_patterns[4] = "Content-Type: ([^[:cntrl:]]+)";
  http_patterns[5] = "Content-Length: ([^[:cntrl:]]+)";
  http_patterns[6] = "Content-Range: ([^[:cntrl:]]+)";
  http_patterns[7] = "Server: ([^[:cntrl:]]+)";
  http_patterns[8] = "Location: ([^[:cntrl:][:space:]]+)";
  /* Very long path in truncated packet */
  http_patterns[9] = "^([A-Z]+) ([^[:cntrl:][:space:]]+)"; 

  /* Remove any parameter information to limit privacy issues */
  http_patterns[10] = "^([^?]+)"; /* To be used with path, Referer and Location when http_full_url==1 */
  http_patterns[11] = "^(https?://[^/]+/?)"; /* To be used with Referer and Location when http_full_url==0 */
  
  for (i=0;i<12;i++)
   {
     regcomp(&http_re[i],http_patterns[i],REG_EXTENDED);
   }

}


void http_init()
{
  /* nothing to do so far */
  init_http_patterns();
}

void *
gethttp(struct udphdr *pudp, int tproto, void *pdir, void *plast)
{
  /* just pass the complete packet and let the tcpL7_flow_stat decide */

  return (void *) pudp;
}

void http_flow_stat(struct ip *pip, void *pproto, int tproto, void *pdir,
		int dir, void *hdr, void *plast)
{
  tcp_pair *ptp;

  void *pdata; /*start of payload */
  int data_length, payload_len;
  tcp_seq seqnum;
  tcb *tcp_stats;
  char last_payload_char;

  tcphdr *ptcp;
  ptcp = (tcphdr *) hdr;

  if (tproto == PROTOCOL_UDP) {
  	  return;
  }

  ptp = ((tcb *) pdir)->ptp;

  if (ptp == NULL)
  	  return;
 
  /* Content of the old FindConType function */

  pdata = (char *) ptcp + ptcp->th_off * 4;
  payload_len = getpayloadlength(pip, plast) - ptcp->th_off * 4;
  data_length = (char *) plast - (char *) pdata + 1;
  seqnum = ntohl(ptcp->th_seq);

  if (data_length <= 0 || payload_len == 0)
    return;

  if (dir == C2S)
    tcp_stats = &(ptp->c2s);
  else
    tcp_stats = &(ptp->s2c);

  if ((char *) pdata + 4 > (char *) plast)
    return;

  /* 
     Minimal operations to count HTTP request methods.
     More complex stuff done only if http_log_complete is enabled
  */

  switch (*((u_int32_t *) pdata)) 
   {
     case GET:
     case POST:
     case HEAD:
       if (dir == C2S)
        { 
          ptp->http_request_count++;
	}
       break;
     case HTTP:
       if (dir == S2C)
        { 
          ptp->http_response_count++;
	}
       break;
     default:
       break;
   }

  /* 
    The only scope of the plugin is to write the log. 
    Skip any elaboration if http_log_complete is disabled.
  */
  
  if (fp_http_logc == NULL || !(LOG_IS_ENABLED(LOG_HTTP_COMPLETE)))
    return;

  char *base = (char *)pdata;

  switch (*((u_int32_t *) pdata)) 
   {
     case GET:
     case POST:
     case HEAD:
       if (dir == C2S)
        { 
          char *http_referer_tail=full_field;
          char *http_url_tail=full_field;
          char *http_ua_tail=full_field;

  	  last_payload_char =  *((char *)(pdata + data_length));
  	  *(char *)(pdata + data_length) = '\0';

	  if (regexec(&http_re[0],base,(size_t) 3,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_method,base+re_res[1].rm_so,
               (msize<9?msize:9));
               http_method[msize<9?msize:9]='\0';

             msize = re_res[2].rm_eo-re_res[2].rm_so;

              memcpy(http_url,base+re_res[2].rm_so,
               (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
               http_url[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
	       
	     base += re_res[2].rm_eo;
	    }
	  else if (regexec(&http_re[9],base,(size_t) 3,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_method,base+re_res[1].rm_so,
               (msize<9?msize:9));
               http_method[msize<9?msize:9]='\0';

             msize = re_res[2].rm_eo-re_res[2].rm_so;

              memcpy(http_url,base+re_res[2].rm_so,
               (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
               http_url[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';

	       if (base+re_res[2].rm_eo>=(char *)plast)
	        {
		  http_url_tail = truncated_field;
	        }
	    }
          else
	   {
	     strcpy(http_method,"-");
	     strcpy(http_url,"-");
	   }

	  if (regexec(&http_re[1],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_host,base+re_res[1].rm_so,
               (msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)));
               http_host[msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)]='\0';
	    }
          else
	   {
	     strcpy(http_host,"-");
	   }

	  if (regexec(&http_re[2],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_referer,base+re_res[1].rm_so,
               (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
               http_referer[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
	       if (base+re_res[1].rm_eo>=(char*)plast)
	        {
		  http_referer_tail = truncated_field;
	        }
	    }
          else
	   {
	     strcpy(http_referer,"-");
	   }

	  if (regexec(&http_re[3],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_ua,base+re_res[1].rm_so,
               (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
               http_ua[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
	      if (base+re_res[1].rm_eo>=(char*)plast)
	       {
		 http_ua_tail = truncated_field;
	       }
	    }
          else
	   {
	     strcpy(http_ua,"-");
	   }

         switch (http_full_url)
	   {
	      case 0:
	        strcpy(http_url_private,"-");

		if (regexec(&http_re[11],http_referer,(size_t) 2,re_res,0)==0)
		{
		  int msize = re_res[1].rm_eo-re_res[1].rm_so;

		  memcpy(http_referer_private,http_referer+re_res[1].rm_so,
		    (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
		  http_referer_private[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
		}   
		else
		{   
	          strcpy(http_referer_private,"-");
		}   
                
                http_referer_tail=full_field;
	        http_url_tail=full_field;

		break;
	      case 1:
		if (regexec(&http_re[10],http_url,(size_t) 2,re_res,0)==0)
		{
		  int msize = re_res[1].rm_eo-re_res[1].rm_so;

		  memcpy(http_url_private,http_url+re_res[1].rm_so,
		    (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
		  http_url_private[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
		}   
		else
		{   
		  strcpy(http_url_private,http_url);
		}   

		if (regexec(&http_re[10],http_referer,(size_t) 2,re_res,0)==0)
		{
		  int msize = re_res[1].rm_eo-re_res[1].rm_so;

		  memcpy(http_referer_private,http_referer+re_res[1].rm_so,
		    (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
		  http_referer_private[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
		}   
		else
		{   
		  strcpy(http_referer_private,http_referer);
		}   
		
                http_referer_tail=full_field;
	        http_url_tail=full_field;

		break;
	      case 2:
	        strcpy(http_url_private,http_url);
	        strcpy(http_referer_private,http_referer);
		break;
	      default:
	        strcpy(http_url_private,"-");
	        strcpy(http_referer_private,"-");
                http_referer_tail=full_field;
	        http_url_tail=full_field;
		break;
	   }

         *(char *)(pdata + data_length) = last_payload_char;

         if (fp_http_logc != NULL && LOG_IS_ENABLED(LOG_HTTP_COMPLETE) )
	  {
	    if (ptp->crypto_src)
               wfprintf (fp_http_logc,"%s\t%s",HostNameEncrypted(ptp->addr_pair.a_address),
                                    ServiceName(ptp->addr_pair.a_port));
	    else
               wfprintf (fp_http_logc,"%s\t%s",HostName(ptp->addr_pair.a_address),
                                    ServiceName(ptp->addr_pair.a_port));
	    if (ptp->crypto_dst)
               wfprintf (fp_http_logc,"\t%s\t%s",HostNameEncrypted(ptp->addr_pair.b_address),
                                    ServiceName(ptp->addr_pair.b_port));
	    else
               wfprintf (fp_http_logc,"\t%s\t%s",HostName(ptp->addr_pair.b_address),
                                    ServiceName(ptp->addr_pair.b_port));
				    
            wfprintf (fp_http_logc,"\t%f",time2double(current_time)/1e6);
  /*          wfprintf (fp_http_logc,"\t%d",seqnum-tcp_stats->syn-1); */
            wfprintf (fp_http_logc,"\t%s\t%s",http_method,http_host);

#ifdef DNS_CACHE_PROCESSOR
              /* DNS server */
                  wfprintf (fp_http_logc, "\t%s",ptp->dns_name!=NULL?ptp->dns_name:"-");
#endif

            wfprintf (fp_http_logc,"\t%s%s\t%s%s\t%s%s",http_url_private,http_url_tail,http_referer_private,http_referer_tail,http_ua,http_ua_tail);

            wfprintf (fp_http_logc,"\n");
	  }
	}
       break;
     case HTTP:
       if (dir == S2C)
        { 
	  char *http_url_tail=full_field;
	  
          if ((*(char *)(pdata+4))!='/')
	   {
	     break;
	   }

  	  last_payload_char =  *((char *)(pdata + data_length));
  	  *(char *)(pdata + data_length) = '\0';

          if ((char *) pdata + 13 <= (char *) plast)
	    {
              memcpy(http_response,(char *) pdata+9,3);
	      http_response[3]='\0';
	      /* At lease one webserver (IdeaWebServer/v0.80) returns an invalid "0" response code
	       producing an invalid http_response[] field.
	       Let's do a sanity check, so that only digits are included
	      */
	      {
		int idx;
		for (idx = 0; idx<3 ; idx++)
		 {
		   if (!isdigit(http_response[idx]))
		    {
		      http_response[idx]='\0';
		      break;
		    }
		 }
		if (strlen(http_response)==0)
                  strcpy(http_response,"-");
	      }
            }
          else
	    {
              strcpy(http_response,"-");
	    }

	  if (regexec(&http_re[4],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_ctype,base+re_res[1].rm_so,
               (msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)));
               http_ctype[msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)]='\0';
	    }
          else
	   {
	     strcpy(http_ctype,"-");
	   }

	  if (regexec(&http_re[5],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_clen,base+re_res[1].rm_so,
               (msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)));
               http_clen[msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)]='\0';
	    }
          else
	   {
	     strcpy(http_clen,"-");
	   }

	  if (regexec(&http_re[6],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_range,base+re_res[1].rm_so,
               (msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)));
               http_range[msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)]='\0';
	    }
          else
	   {
	     strcpy(http_range,"-");
	   }

	  if (regexec(&http_re[7],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_server,base+re_res[1].rm_so,
               (msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)));
               http_server[msize<(HTTP_SMALL_BUFFER_SIZE-1)?msize:(HTTP_SMALL_BUFFER_SIZE-1)]='\0';
	    }
          else
	   {
	     strcpy(http_server,"-");
	   }

	  if (regexec(&http_re[8],base,(size_t) 2,re_res,0)==0)
            {
             int msize = re_res[1].rm_eo-re_res[1].rm_so;

              memcpy(http_url,base+re_res[1].rm_so,
               (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
               http_url[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
	       if (base+re_res[1].rm_eo>=(char*)plast)
	        {
		  http_url_tail = truncated_field;
	        }
	    }
          else
	   {
	     strcpy(http_url,"-");
	   }

          switch (http_full_url)
	   {
	      case 0:
		if (regexec(&http_re[11],http_url,(size_t) 2,re_res,0)==0)
		{
		  int msize = re_res[1].rm_eo-re_res[1].rm_so;

		  memcpy(http_url_private,http_url+re_res[1].rm_so,
		    (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
		  http_url_private[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
		}
		else
		{
		  strcpy(http_url_private,"-");
		}
		
	        http_url_tail=full_field;
		break;
	      case 1:
		if (regexec(&http_re[10],http_url,(size_t) 2,re_res,0)==0)
		{
		  int msize = re_res[1].rm_eo-re_res[1].rm_so;

		  memcpy(http_url_private,http_url+re_res[1].rm_so,
		    (msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)));
		  http_url_private[msize<(HTTP_LARGE_BUFFER_SIZE-1)?msize:(HTTP_LARGE_BUFFER_SIZE-1)]='\0';
		}
		else
		{
		  strcpy(http_url_private,http_url);
		}
		
	        http_url_tail=full_field;
		break;
	      case 2:
		strcpy(http_url_private,http_url);
		break;
	      default:
		strcpy(http_url_private,"-");
	        http_url_tail=full_field;
		break;
	   } 
	  
          *(char *)(pdata + data_length) = last_payload_char;

          if (fp_http_logc != NULL && LOG_IS_ENABLED(LOG_HTTP_COMPLETE) )
	   {
	    if (ptp->crypto_src)
               wfprintf (fp_http_logc,"%s\t%s",HostNameEncrypted(ptp->addr_pair.a_address),
                                    ServiceName(ptp->addr_pair.a_port));
	    else
               wfprintf (fp_http_logc,"%s\t%s",HostName(ptp->addr_pair.a_address),
                                    ServiceName(ptp->addr_pair.a_port));
	    if (ptp->crypto_dst)
               wfprintf (fp_http_logc,"\t%s\t%s",HostNameEncrypted(ptp->addr_pair.b_address),
                                    ServiceName(ptp->addr_pair.b_port));
	    else
               wfprintf (fp_http_logc,"\t%s\t%s",HostName(ptp->addr_pair.b_address),
                                    ServiceName(ptp->addr_pair.b_port));
				    
             wfprintf (fp_http_logc,"\t%f",time2double(current_time)/1e6);
     /*      wfprintf (fp_http_logc,"\t%d",seqnum-tcp_stats->syn-1); */
             wfprintf (fp_http_logc,"\t%s\t%s\t%s\t%s","HTTP",http_response,http_clen,http_ctype);
             wfprintf (fp_http_logc,"\t%s\t%s\t%s%s",http_server,http_range,http_url_private,http_url_tail);

             wfprintf (fp_http_logc,"\n");
	   }
	 }
       break;
     default:
       break;
   }

  return;
}

void make_http_conn_stats(void *thisflow, int tproto)
{
  return;
}
