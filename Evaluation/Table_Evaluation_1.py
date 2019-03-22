import os

if __name__ == '__main__':
    
    methods = [
            'SCALAR',
            'SSE',
            'AVX',
            'MASK_AVX',
            'OGL'
            ]
    
    values = [
            'FPS',
            'Percentage culled',
            'Draw Calls',
            'Culling time',
            'DepthTest Time',
            'Rasterize Time'
            ]
    
    table_description_1 = [
            'SCALAR',
            'SSE',
            'AVX',
            'Masked',
            'OGL'
            ]
    
    table_description_2 = [
            'FPS',
            'Tries culled (\%)',
            'Draw Calls',
            'Culling t (ms)',
            'Depth Test t (ms)',
            'Rasterizer t (ms)'
            ]
    
    evaluation = 'Evaluation_1'
    
    lines = []
    for value in values:
        lines.append('')
    
    if not os.path.isdir('Tabellen'):
        os.mkdir('Tabellen')
    
    output = open('Tabellen/' + evaluation + '_Tabelle', 'w')
    
    output.write('\hline\n')
    output.write('{:<18} '.format(''))
    for description in table_description_1:
        output.write('& {:<10}'.format(description))
    output.write('\\\\ \hline\n')
    
    for i in range(len(table_description_2)):
        lines[i]+='{:<18} '.format(table_description_2[i])
    
    for method in methods:
        for i in range(len(values)):
            if values[i] != 'Percentage culled':
                f = evaluation + '/' + method + '_plots/' + method + '_' + values[i] + '_Data'
                sum = 0
                counter = 0
                with open(f) as f:
                    next(f)
                    next(f)
                    for line in f.read().split('\n'):
                        if line == '':
                            break
                        sum+=float(line)
                        counter+=1
                    
                    if values[i] == 'Culling time' or values[i] == 'DepthTest Time' or values[i] == 'Rasterize Time':
                        print('{:<10} {:<16} {:>8} ms'.format(method, values[i], round(1000*(sum/counter),2)))
                        lines[i]+='& {:<10}'.format(round(1000*(sum/counter),2))
                    elif values[i] == 'Draw Calls':
                        print('{:<10} {:<16} {:>8}'.format(method, values[i], round(sum/counter)))
                        lines[i]+='& {:<10}'.format(round(sum/counter))
                    else:
                        print('{:<10} {:<16} {:>8}'.format(method, values[i], round(sum/counter,2)))
                        lines[i]+='& {:<10}'.format(round(sum/counter,2))
            else:
                f_culled = evaluation + '/' + method + '_plots/' + method + '_Culled Tries_Data'
                f_visible = evaluation + '/' + method + '_plots/' + method + '_Visible Tries_Data'
                culled = []
                visible = []
                with open(f_culled) as f_culled:
                    next(f_culled)
                    next(f_culled)
                    for line in f_culled.read().split('\n'):
                        if line == '':
                            break
                        culled.append(float(line))
                with open(f_visible) as f_visible:
                    next(f_visible)
                    next(f_visible)
                    for line in f_visible.read().split('\n'):
                        if line == '':
                            break
                        visible.append(float(line))
                
                sum = 0
                counter = 0
                for j in range(len(culled)):
                    percentage = culled[j]/(culled[j]+visible[j])
                    sum+=float(percentage)
                    counter+=1
                print('{:<10} {:<16} {:>8} %'.format(method, '% Culled', round(100*(sum/counter),2)))
                lines[i]+='& {:<10}'.format(round(100*(sum/counter),2))
        print()
    
    for i in range(len(lines)):
         lines[i]+='\\\\'
    for i in range(len(lines)-1):
        lines[i]+=' \hline'
    for i in range(len(lines)):  
        output.write(lines[i] + '\n')
    output.write('\hline')