% Matlab table generator
% Gabotronics - September 2018

% Open files
fileID_h = fopen('tables.h','w');
fileID_c = fopen('tables.c','w');

fprintf(fileID_h,'#ifndef TABLES_H\n#define TABLES_H\n\n');
fprintf(fileID_h,'// Gabotronics - September 2018\n');
fprintf(fileID_h,'// Constants tables\n');
fprintf(fileID_h,'// This file was generated with MATLAB using tables.m\n\n');
fprintf(fileID_h,'#include \"stdint.h\"\n');
fprintf(fileID_h,'#include \"<avr/pgmspace.h>\"\n\n');

fprintf(fileID_c,'#include \"tables.h\"\n\n');
fprintf(fileID_c,'// Gabotronics - September 2018\n');
fprintf(fileID_c,'// Constants tables\n');
fprintf(fileID_c,'// This file was generated with MATLAB using tables.m\n');

% SINE WAVE
% Only need one table for the sine wave, with 1024 points
f=0;    % Index of sinewave
N = 1024;
name = FuncName(f);
fprintf(fileID_h,'extern const int8_t %s[%d];\n',name,N);
fprintf(fileID_c,'\nconst int8_t %s[%d] PROGMEM = {',name,N);
for n=0:N-1
    if(rem(n,16)==0)    % 16 constants per line
        fprintf(fileID_c,'\n    ');
    end
    value = FuncOut(f,n,N);
    fprintf(fileID_c,'%4d, ',value);
end
fprintf(fileID_c,'\n};\n');

% EXPONENTIAL WAVE
f=4;    % Index of sinewave
N = 1024;
name = FuncName(f);
fprintf(fileID_h,'extern const int8_t %s[%d];\n',name,N);
fprintf(fileID_c,'\nconst int8_t %s[%d] PROGMEM = {',name,N);
% First half
for n=0:N/2-1
    if(rem(n,16)==0)    % 16 constants per line
        fprintf(fileID_c,'\n    ');
    end
    value = FuncOut(f,N/2-1-n,N);
    fprintf(fileID_c,'%4d, ',value);
end
% Second half
for n=0:N/2-1
    if(rem(n,16)==0)    % 16 constants per line
        fprintf(fileID_c,'\n    ');
    end
    value = -FuncOut(f,N/2-1-n,N);
    fprintf(fileID_c,'%4d, ',value);
end
fprintf(fileID_c,'\n};\n');

% FFT WINDOWS
% Only need half the window of N=1024
for f = 1:3     % Cycle through remaining functions
    N = 1024;
    name = FuncName(f);
    fprintf(fileID_h,'extern const int8_t %s[%d];\n',name,N/2);
    fprintf(fileID_c,'\nconst int8_t %s[%d] PROGMEM = {',name,N/2);
    for n=0:N/2-1
        if(rem(n,16)==0)    % 16 constants per line
            fprintf(fileID_c,'\n    ');
        end
        value = FuncOut(f,n,N);
        fprintf(fileID_c,'%4d, ',value);
    end
    fprintf(fileID_c,'\n};\n');
end

% BIT REVERSED
for k = 8:12     % Tables for 2^8 through 2^12
    N = 2^k;
    name = FuncName(5);
    if(k<=8)
        fprintf(fileID_h,'extern const int8_t %s%d[%d];\n',name,N,N);
        fprintf(fileID_c,'\nconst int8_t %s%d[%d] PROGMEM = {',name,N,N);
    else
        fprintf(fileID_h,'extern const int16_t %s%d[%d];\n',name,N,N);
        fprintf(fileID_c,'\nconst int16_t %s%d[%d] PROGMEM = {',name,N,N);
    end
    for n=0:N-1
        if(rem(n,16)==0)    % 16 constants per line
            fprintf(fileID_c,'\n    ');
        end
        value = FuncOut(5,n,N);
        fprintf(fileID_c,'%4d, ',value);
    end
    fprintf(fileID_c,'\n};\n');
end

fprintf(fileID_h,'\n#endif');
fprintf(fileID_c,'\n');

% Close files
fclose(fileID_h);
fclose(fileID_c);

% Plot in MATLAB
N=1024;
n = 0:1:N/2-1;
data=FuncOut(4,n,N);
stem(data);

% Function output
function output = FuncOut(function_id,input,size)
    scale = 127;
    switch function_id
        case 0  % Sinewave
            output = sin(2*pi*input/size);
        case 1 % Hamming window
            output = 0.53836-0.46164*cos(2*pi*input/(size-1));
        case 2 % Hann window
            output = 0.5*(1-cos(2*pi*input/(size-1)));
        case 3 %Blackman window
            output = 0.42 -0.5*cos(2*pi*input/(size-1)) +0.08*cos(4*pi*input/(size-1));
        case 4 % Exponential (manually adjusted)
            output = 1.01-exp((input-(size/2.29))/91.5);
        case 5 % Bit reversed table
            x = (0:size-1)';        % Algorithm not efficient, the entire
            v = bitrevorder(x);     % bit reversed table is created on
            output = v(input+1);    % every call. It's ok since I only
            return                  % need to use the script once... :-)
        otherwise
            output = 0;
    end
    output = round(output*scale);
end

% Function name
function name = FuncName(function_id)
    switch function_id
        case 0
            name = 'Sine';
        case 1
            name = 'Hamming';
        case 2
            name = 'Hann';
        case 3
            name = 'Blackman';
        case 4
            name = 'Exponential';
        case 5
            name = "BitReverse";
        otherwise
            name = 'invalid';
    end
end


