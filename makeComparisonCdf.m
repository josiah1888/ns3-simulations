function []=makeComparisonCdf(gpsr, hwmp, output)
clc;

close all;

x = dlmread(gpsr);

x = x.* 8 ./(1024 * 1024);

y = dlmread(hwmp);

y = y.* 8 ./(1024 * 1024); 

fsize = 32;

figure;

hold all;

h1 = cdfplot(x(:,2));

set(h1,'color',[0,0,0],'LineWidth',3,'LineStyle','-');%,'Marker','d','MarkerSize',10);

h = cdfplot(y(:,2));

set(h,'color',[0,0,0],'LineWidth',3,'LineStyle','--');%,'Marker','o','MarkerSize',10);

xlim([0 5.05]);

ylim([0 1.05])

xlabel('Throughput, Mbps','fontname','Times New Roman','fontsize',fsize); ylabel('Time Fraction','fontname','Times New Roman','fontsize',fsize);

set(gca,'Xtick',linspace(0,5,6))

set(gca,'Ytick',linspace(0,1,6))

legend('GPSR', 'HWMP');%, 'depth', 'color', 'skeletal'

set(legend,'Location','southeast');

legend boxoff;

set(gca,'fontname','Times New Roman');

set(gca,'fontsize',fsize);

%title('5% failures')

title('');

grid off;
hold off;

saveas(h1, output)

exit;
end
