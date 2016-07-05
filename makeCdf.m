function []=makeCdf(input, output)
clc;

close all;

x = dlmread(input);

x = x.* 8 ./(1024 * 1024);

%load('patient_all.mat');

%avg filter for 1000 points (per each second)
%{
for i=1:size(y, 1)

    if(i<=size(y, 1)-99)
        y(i,:)=sum(y(i:i+99,:))/100;

    else

        y(i,:)=sum(y(i-99:i,:))/100;

    end;

end;

 

%avg filter for 1000 points (per each second)
for i=1:size(x, 1)

    if(i<=size(x, 1)-99)

        x(i,:)=sum(x(i:i+99,:))/100;

    else

        x(i,:)=sum(x(i-99:i,:))/100;

    end;

end;
%}
 

 

fsize = 32;

figure;

hold on;

h1 = cdfplot(x(:,2));

set(h1,'color',[0,0,0],'LineWidth',3,'LineStyle','-');%,'Marker','d','MarkerSize',10);

 

%h = h1 %cdfplot(y:2);

%set(h,'color',[0,0,0],'LineWidth',3,'LineStyle','--');%,'Marker','o','MarkerSize',10);

 

 

%

% h2 = cdfplot(y2);

% set(h2,'color',[0,0,0]+0.6,'LineWidth',2,'LineStyle','-');%,'Marker','d','MarkerSize',10);

%

% h3 = cdfplot(y3);

% set(h3,'color',[0,0,0],'LineWidth',2,'LineStyle','--');%,'Marker','o','MarkerSize',10);

 

xlim([0 5.05]);

ylim([0 1.05])

 

 

xlabel('Throughput, Mbps','fontname','Times New Roman','fontsize',fsize); ylabel('Time Fraction','fontname','Times New Roman','fontsize',fsize);

 

%numberOfXTicks = 5;

set(gca,'Xtick',linspace(0,5,6))

set(gca,'Ytick',linspace(0,1,6))

 

legend('GPSR');%, 'depth', 'color', 'skeletal'

set(legend,'Location','southeast');

legend boxoff;

set(gca,'fontname','Times New Roman');

set(gca,'fontsize',fsize);

 

 

%title('5% failures')

title('');

grid off;

saveas(h1,output)

exit;
end
