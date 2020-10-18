// -----------------------------------------------------------------------------------
// Misc functions to help with commands, etc.
#pragma once

// string to int with error checking
bool atoi2(char *a, int *i, bool sign=true) {
  int len=strlen(a);
  if (len == 0 || len > 6) return false;
  for (int l=0; l < len; l++) {
    if ((l == 0) && ((a[l] == '+') || (a[l] == '-')) && sign) continue;
    if ((a[l] < '0') || (a[l] > '9')) return false;
  }
  long l=atol(a);
  if ((l < -32767) || (l > 32768)) return false;
  *i=l;
  return true;
}

// string to float with error checking
bool atof2(char *a, double *d, bool sign=true) {
  int dc=0;
  int len=strlen(a);
  for (int l=0; l < len; l++) {
    if ((l == 0) && ((a[l] == '+') || (a[l] == '-')) && sign) continue;
    if (a[l] == '.') { if (dc == 0) { dc++; continue; } else return false; }
    if ((a[l] < '0') || (a[l] > '9')) return false;
  }
  *d=atof(a);
  return true;
}

char serialRecvFlush() {
  char c=0;
  while (Ser.available()>0) c=Ser.read();
  return c;
}

// smart LX200 aware command and response over serial
boolean processCommand(const char cmd[], char response[], long timeOutMs) {
  Ser.setTimeout(timeOutMs);
  
  // clear the read/write buffers
  Ser.flush();
  serialRecvFlush();

  // send the command
  Ser.print(cmd);

  boolean noResponse=false;
  boolean shortResponse=false;
  if ((cmd[0]==(char)6) && (cmd[1]==0)) shortResponse=true;
  if ((cmd[0]==':') || (cmd[0]==';')) {
    if (cmd[1]=='G') {
      if (strchr("RDE",cmd[2])) { if (timeOutMs < 300) timeOutMs = 300; }
    } else
    if (cmd[1]=='M') {
      if (strchr("ewnsg",cmd[2])) noResponse=true;
      if (strchr("SAP",cmd[2])) shortResponse=true;
    } else
    if (cmd[1]=='Q') {
      if (strchr("#ewns",cmd[2])) noResponse=true;
    } else
    if (cmd[1]=='A') {
      if (strchr("W123456789+",cmd[2])) { shortResponse=true; if (timeOutMs < 1000) timeOutMs = 1000; }
    } else
    if ((cmd[1]=='F') || (cmd[1]=='f')) {
      if (strchr("+-QZHhF1234",cmd[2])) noResponse=true;
      if (strchr("Ap",cmd[2])) shortResponse=true;
    } else
    if (cmd[1]=='r') {
      if (strchr("+-PRFC<>Q1234",cmd[2])) noResponse=true;
      if (strchr("~S",cmd[2])) shortResponse=true;
    } else
    if (cmd[1] == 'R') {
      if (strchr("AEGCMS0123456789", cmd[2])) noResponse=true;
    } else
    if (cmd[1]=='S') {
      if (strchr("CLSGtgMNOPrdhoTBX",cmd[2])) shortResponse=true;
    } else
    if (cmd[1]=='L') {
      if (strchr("BNCDL!",cmd[2])) noResponse=true;
      if (strchr("o$W",cmd[2])) { shortResponse=true; if (timeOutMs < 1000) timeOutMs = 1000; }
    } else
    if (cmd[1]=='B') {
      if (strchr("+-",cmd[2])) noResponse=true;
    } else
    if (cmd[1]=='C') {
      if (strchr("S",cmd[2])) noResponse=true;
    } else
    if (cmd[1]=='h') {
      if (strchr("FC",cmd[2])) { noResponse=true; if (timeOutMs < 1000) timeOutMs = 1000; }
      if (strchr("QPR",cmd[2])) { shortResponse=true; if (timeOutMs < 300) timeOutMs = 300; }
    } else
    if (cmd[1]=='T') {
      if (strchr("QR+-SLK",cmd[2])) noResponse=true;
      if (strchr("edrn",cmd[2])) shortResponse=true;
    } else
    if (cmd[1]=='U') {
      noResponse=true; 
    } else
    if ((cmd[1]=='W') && (cmd[2]!='?')) { 
      noResponse=true; 
    } else
    if ((cmd[1]=='$') && (cmd[2]=='Q') && (cmd[3]=='Z')) {
      if (strchr("+-Z/!",cmd[4])) noResponse=true;
    }

    // override for checksum protocol
    if (cmd[0]==';') { noResponse=false; shortResponse=false; }
  }

  unsigned long timeout=millis()+(unsigned long)timeOutMs;
  if (noResponse) {
    response[0]=0;
    return true;
  } else
  if (shortResponse) {
    while ((long)(timeout-millis()) > 0) {
      if (Ser.available()) { response[Ser.readBytes(response,1)]=0; break; }
    }
    return (response[0]!=0);
  } else {
    // get full response, '#' terminated
    int responsePos=0;
    char b=0;
    while ((long)(timeout-millis()) > 0 && b != '#') {
      if (Ser.available()) {
        b=Ser.read();
        response[responsePos]=b; responsePos++; if (responsePos>39) responsePos=39; response[responsePos]=0;
      }
    }
    return (response[0]!=0);
  }
}

bool command(const char command[], char response[]) {
  bool success = processCommand(command,response,webTimeout);
  int l=strlen(response)-1; if (l >= 0 && response[l] == '#') response[l]=0;
  return success;
}

bool commandBlind(const char command[]) {
  char response[40]="";
  return processCommand(command,response,webTimeout);
}

bool commandEcho(const char command[]) {
  char response[40]="";
  char c[40]="";
  sprintf(c,":EC%s#",command);
  return processCommand(c,response,webTimeout);
}

bool commandBool(const char command[]) {
  char response[40]="";
  bool success = processCommand(command,response,webTimeout);
  int l=strlen(response)-1; if (l >= 0 && response[l] == '#') response[l]=0;
  if (!success) return false;
  if (response[1] != 0) return false;
  if (response[0] == '0') return false; else return true;
}

char *commandString(const char command[]) {
  static char response[40]="";
  bool success = processCommand(command,response,webTimeout);
  int l=strlen(response)-1; if (l >= 0 && response[l] == '#') response[l]=0;
  if (!success) strcpy(response,"?");
  return response;
}


// convert string in format HH:MM:SS to double
// (also handles)           HH:MM.M
// (also handles)           HH:MM:SS
// (also handles)           HH:MM:SS.SSSS
bool hmsToDouble(double *f, char *hms) {
  char h[3],m[5];
  int  h1,m1,m2=0;
  double s1=0;

  while (*hms == ' ') hms++; // strip prefix white-space

  if (strlen(hms) > 13) hms[13]=0; // limit maximum length
  int len=strlen(hms);
  
  if (len != 8 && len < 10) return false;

  // convert the hours part
  h[0]=*hms++; h[1]=*hms++; h[2]=0; if (!atoi2(h,&h1,false)) return false;

  // make sure the seperator is an allowed character, then convert the minutes part
  if (*hms++ != ':') return false;
  m[0]=*hms++; m[1]=*hms++; m[2]=0; if (!atoi2(m,&m1,false)) return false;

  // make sure the seperator is an allowed character, then convert the seconds part
  if (*hms++ != ':') return false;
  if (!atof2(hms,&s1,false)) return false;
  
  if (h1 < 0 || h1 > 23 || m1 < 0 || m1 > 59 || m2 < 0 || m2 > 9 || s1 < 0 || s1 > 59.9999) return false;

  *f=(double)h1+(double)m1/60.0+(double)m2/600.0+s1/3600.0;
  return true;
}

// convert double to string in a variety of formats (as above) 
void doubleToHms(char *reply, double *f) {
  double h1,m1,f1,s1,sd=0;

  // round to 0.00005 second or 0.5 second, depending on precision mode
  f1=fabs(*f)+0.000139;

  h1=floor(f1);
  m1=(f1-h1)*60.0;
  s1=(m1-floor(m1))*60.0;

  // finish off calculations for hms and form string template
  char s[]="%s%02d:%02d:%02d.%04d";
  s[16]=0;

  // set sign and return result string
  char sign[2]="";
  if ((sd != 0 || s1 != 0 || m1 != 0 || h1 != 0) && *f < 0.0) strcpy(sign,"-");
  sprintf(reply,s,sign,(int)h1,(int)m1,(int)s1);
}

// convert string in format sDD:MM:SS to double
// (also handles)           sDD:MM:SS.SSS
//                          DDD:MM:SS
//                          sDD:MM
//                          DDD:MM
//                          sDD*MM
//                          DDD*MM
bool dmsToDouble(double *f, char *dms, bool sign_present) {
  char d[4],m[5];
  int d1,m1,lowLimit=0,highLimit=360,len;
  double s1=0,sign=1;
  bool secondsOff=false;

  while (*dms == ' ') dms++; // strip prefix white-space
  if (strlen(dms) > 13) dms[13]=0; // maximum length
  len=strlen(dms);

  if (len != 9 && len < 11) return false;

  // determine if the sign was used and accept it if so, then convert the degrees part
  if (sign_present) {
    if (*dms == '-') sign=-1.0; else if (*dms == '+') sign=1.0; else return false; 
    dms++; d[0]=*dms++; d[1]=*dms++; d[2]=0; if (!atoi2(d,&d1,false)) return false;
  } else {
    d[0]=*dms++; d[1]=*dms++; d[2]=*dms++; d[3]=0; if (!atoi2(d,&d1,false)) return false;
  }

  // make sure the seperator is an allowed character, then convert the minutes part
  if (*dms != ':' && *dms != '*' && *dms != char(223)) return false; else dms++;
  m[0]=*dms++; m[1]=*dms++; m[2]=0; if (!atoi2(m,&m1,false)) return false;

  if (!secondsOff) {
    // make sure the seperator is an allowed character, then convert the seconds part
    if (*dms++ != ':' && *dms++ != '\'') return false;
    if (!atof2(dms,&s1,false)) return false;
  }

  if (sign_present) { lowLimit=-90; highLimit=90; }
  if ((d1 < lowLimit) || (d1 > highLimit) || (m1 < 0) || (m1 > 59) || (s1 < 0) || (s1 > 59.999)) return false;

  *f=sign*((double)d1+(double)m1/60.0+s1/3600.0);
  return true;
}

boolean doubleToDms(char *reply, double *f, boolean fullRange, boolean signPresent) {
  char sign[]="+";
  int  o=0,d1,s1=0;
  double m1,f1;
  f1=*f;

  // setup formatting, handle adding the sign
  if (f1<0) { f1=-f1; sign[0]='-'; }

  f1=f1+0.000139; // round to 1/2 arc-second
  d1=floor(f1);
  m1=(f1-d1)*60.0;
  s1=(m1-floor(m1))*60.0;
  
  char s[]="+%02d*%02d:%02d";
  if (signPresent) { 
    if (sign[0]=='-') { s[0]='-'; } o=1;
  } else {
    strcpy(s,"%02d*%02d:%02d");
  }
  if (fullRange) s[2+o]='3';
 
  sprintf(reply,s,d1,(int)m1,s1);
  return true;
}

uint8_t timeToByte(float t) {
  float v = 10;                         // default is 1 second
  if (t <= 0.0162) v=0; else            // 0.0156 (1/64 second)        (0)
  if (t <= 0.0313) v=1; else            // 0.0313 (1/32 second)        (1)
  if (t <= 0.0625) v=2; else            // 0.0625 (1/16 second)        (2)
  if (t <= 1.0) v=2.0+t*8.0; else       // 0.125 seconds to 1 seconds  (2 to 10)
  if (t <= 10.0) v=6.0+t*4.0; else      // 0.25 seconds to 10 seconds  (10 to 46)
  if (t <= 30.0) v=26.0+t*2.0; else     // 0.5 seconds to 30 seconds   (46 to 86)
  if (t <= 120.0) v=56.0+t; else        // 1 second to 120 seconds     (86 to 176)
  if (t <= 600.0) v=168.0+t/15.0; else  // 15 seconds to 300 seconds   (176 to 208)
  if (t <= 3360.0) v=198.0+t/60.0; else // 1 minute to 56 minutes      (208 to 254)
  if (t <= 3600.0) v=255;               // 1 hour                      (255)
  if (v < 0) v=0;
  if (v > 255) v=255;
  return lround(v);
}

float byteToTime(uint8_t b) {
  float v = 1.0;                        // default is 1 second
  if (b == 0) v=0.016125; else          // 0.0156 (1/64 second)        (0)
  if (b == 1) v=0.03125; else           // 0.0313 (1/32 second)        (1)
  if (b == 2) v=0.0625; else            // 0.0625 (1/16 second)        (2)
  if (b <= 10) v=(b-2.0)/8.0; else      // 0.125 seconds to 1 seconds  (2 to 10)
  if (b <= 46) v=(b-6.0)/4.0; else      // 0.25 seconds to 10 seconds  (10 to 46)
  if (b <= 86) v=(b-26.0)/2.0; else     // 0.5 seconds to 30 seconds   (46 to 86)
  if (b <= 176) v=(b-56.0); else        // 1 second to 120 seconds     (86 to 176)
  if (b <= 208) v=(b-168.0)*15.0; else  // 15 seconds to 300 seconds   (176 to 208)
  if (b <= 254) v=(b-198.0)*60.0; else  // 1 minute to 56 minutes      (208 to 254)
  if (b == 255) v=3600.0;               // 1 hour                      (255)
  return v;
}

// remove leading and trailing 0's
void stripNum(char s[]) {
  int pp=-1;
  for (unsigned int p=0; p < strlen(s); p++) if (s[p] == '.') { pp=p; break; }
  if (pp != -1) {
    int p;
    for (p=strlen(s)-1; p >= pp; p--) { if (s[p] != '0') break; s[p]=0; }
    if (s[p] == '.') s[p]=0;
  }
  while (s[0] == '0' && s[1] != '.' && strlen(s) > 1) memmove(&s[0],&s[1],strlen(s));
}
