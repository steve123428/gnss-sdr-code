%% Plot Code Phase vs. TOW for Each Telemetry File Separately
% Author: Jiwoo Suh
% Description: Finds all Telemetry*.mat files and plots one figure per file

clear; clc; close all;

%% Parameters
fs = 4e6;                        % Sampling frequency [Hz]
samples_per_code = fs * 1e-3;    % 1 ms GPS C/A code = 4000 samples

%% Automatically find all telemetry files
file_struct = dir('Telemetry*.mat');
file_list = {file_struct.name};

if isempty(file_list)
    error('No Telemetry*.mat files found in the current directory.');
end

fprintf('Found %d telemetry files:\n', numel(file_list));
disp(file_list');

%% Process each file and plot separately
for k = 1:numel(file_list)
    fname = file_list{k};
    data = load(fname);

    if isfield(data, 'TOW_at_current_symbol_ms') && isfield(data, 'tracking_sample_counter')
        TOW = double(data.TOW_at_current_symbol_ms);
        sample_counter = double(data.tracking_sample_counter);
        
        % Compute normalized code phase (fraction within 1 ms code)
        code_phase = mod(sample_counter / samples_per_code, 1);
        
        % Create figure for this telemetry file
        figure('Color', 'w');
        plot(TOW, code_phase, 'b.-', 'LineWidth', 1.2, 'MarkerSize', 8);
        xlabel('TOW at Current Symbol [ms]', 'FontSize', 12);
        ylabel('Normalized Code Phase (0–1)', 'FontSize', 12);
        title(sprintf('Code Phase vs. TOW: %s', fname), 'Interpreter', 'none', 'FontSize', 14);
        grid on;
        xlim([min(TOW), max(TOW)]);
        
        % Optional: save each figure automatically
        % saveas(gcf, sprintf('%s_code_phase.png', fname(1:end-4)));
    else
        warning('Variables not found in %s', fname);
    end
end