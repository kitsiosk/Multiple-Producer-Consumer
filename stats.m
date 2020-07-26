type = 'con';
fnames={sprintf('%sArr1.txt', type), sprintf('%sArr01.txt', type), sprintf('%sArr001.txt', type),  sprintf('%sArr.txt', type)};
titles = {'T=1sec', 'T=0.1sec', 'T=0.01sec', 'T={1, 0.1, 0.01}sec'};
for i=1:length(fnames)
    subplot(2, 2, i);
    
    fid = fopen(fnames{i});
    M = textscan(fid, '%f', 'Delimiter', ',');
    data = M{1};
    fclose(fid);
    
    if i==4
        % Cut out zeros
        data = data(1:1332);
    end
    plot(data)
    hold on
    title(titles{i});
    set(gca,'xticklabel',{[]})
    if (i==1 || i==3 || i==4) && strcmp(type, 'prod')
        ylim([0 5])
    end
    if i==2 && strcmp(type, 'con')
        ylim([0 100])
    end
    if i==4 && strcmp(type, 'con')
        ylim([0 100])
    end
    if i==3
        xlim([0 9000])
    end
    if i==4
        xlim([0 1332])
    end

    mV = mean(data);
    stdV = std(data);
    medV = median(data);
    minV = min(data);
    maxV = max(data);
    plot([0 length(data)], [mV, mV])
    hold off

    fprintf('Mean: %0.2f\nMedian: %d\nStd: %0.2f\nMin: %d\nMax: %d\n', mV, medV, stdV, minV, maxV);
end