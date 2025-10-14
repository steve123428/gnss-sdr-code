function update_all_acq_max(pattern)
% update_all_acq_max.m
% Usage:
%   update_all_acq_max            % processes 'acq_dump*.mat'
%   update_all_acq_max('acq_dump*_4_sat_25.mat')  % custom pattern
%
% For each .mat: expects variable 'acq_grid'. Appends:
%   maxVal, linIdx, row, col

if nargin < 1 || isempty(pattern)
    pattern = 'acq_dump*.mat';
end

files = dir(pattern);
if isempty(files)
    error('No files match pattern: %s', pattern);
end

fprintf('Found %d files. Processing...\n', numel(files));

for k = 1:numel(files)
    fname = files(k).name;
    try
        % Check contents quickly
        vars = whos('-file', fname);
        hasGrid = any(strcmp({vars.name}, 'acq_grid'));
        if ~hasGrid
            warning('Skipping %s (no variable acq_grid).', fname);
            continue;
        end

        % Load only acq_grid
        S = load(fname, 'acq_grid');
        A = S.acq_grid;

        % Compute max and its position
        [maxVal, linIdx] = max(A(:));
        [row, col] = ind2sub(size(A), linIdx);

        % Append results
        save(fname, 'maxVal', 'linIdx', 'row', 'col', '-append');

        fprintf('OK  : %s  | maxVal=%g at (row=%d, col=%d)\n', fname, maxVal, row, col);

    catch ME
        warning('FAIL: %s -> %s', fname, ME.message);
    end
end

fprintf('Done.\n');
end