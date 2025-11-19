% === CONFIG ===
dirA = fullfile(getenv('HOME'), 'gnss-sdr', 'work');        % receiver 1
dirB = fullfile(getenv('HOME'), 'Desktop', 'PVT_backup');   % receiver 2

% === Discover PRNs automatically from filenames ===
filesA = dir(fullfile(dirA, 'PVT_solution_PRN*.mat'));
filesB = dir(fullfile(dirB, 'PVT_solution_PRN*.mat'));

% Safely extract PRN numbers from filenames
namesA = {filesA.name};
namesB = {filesB.name};

prnsA = cellfun(@(s) sscanf(s, 'PVT_solution_PRN%d.mat', 1), namesA);
prnsB = cellfun(@(s) sscanf(s, 'PVT_solution_PRN%d.mat', 1), namesB);

% Remove any zeros (failed sscanf) just in case
prnsA = prnsA(prnsA ~= 0);
prnsB = prnsB(prnsB ~= 0);

prn_list = intersect(prnsA, prnsB);   % only PRNs present in BOTH dirs
prn_list = unique(prn_list);          % just in case

fprintf('Comparing PRNs: %s\n', sprintf('%d ', prn_list));

% Preallocate as empty struct array
results = struct('PRN', {}, ...
                 'TOW_ms', {}, ...
                 'pseudorange_A', {}, ...
                 'pseudorange_B', {}, ...
                 'pseudorange_error', {}, ...
                 'carrier_phase_A', {}, ...
                 'carrier_phase_B', {}, ...
                 'carrier_phase_error', {});

for idx = 1:numel(prn_list)
    prn = prn_list(idx);
    fname = sprintf('PVT_solution_PRN%d.mat', prn);

    fileA = fullfile(dirA, fname);
    fileB = fullfile(dirB, fname);

    if ~isfile(fileA)
        fprintf('PRN %d: file missing in A: %s\n', prn, fileA);
        continue;
    end
    if ~isfile(fileB)
        fprintf('PRN %d: file missing in B: %s\n', prn, fileB);
        continue;
    end

    A = load(fileA);
    B = load(fileB);

    tA = A.TOW_at_current_symbol_ms(:).';
    tB = B.TOW_at_current_symbol_ms(:).';

    prA = A.Pseudorange_m(:).';
    prB = B.Pseudorange_m(:).';

    phA = A.Carrier_phase_rads(:).';
    phB = B.Carrier_phase_rads(:).';

    % Align by TOW (ms) – using rounding to handle float noise
    tA_rounded = round(tA);
    tB_rounded = round(tB);

    [t_common, idxA, idxB] = intersect(tA_rounded, tB_rounded);

    % If there are NO intersecting TOWs → no record at all
    if isempty(t_common)
        fprintf('PRN %d: no overlapping TOW between receivers, skipping\n', prn);
        continue;
    end

    pr_err = prA(idxA) - prB(idxB);
    ph_err = phA(idxA) - phB(idxB);

    k = numel(results) + 1;
    results(k).PRN                 = prn;
    results(k).TOW_ms              = t_common;
    results(k).pseudorange_A       = prA(idxA);
    results(k).pseudorange_B       = prB(idxB);
    results(k).pseudorange_error   = pr_err;
    results(k).carrier_phase_A     = phA(idxA);
    results(k).carrier_phase_B     = phB(idxB);
    results(k).carrier_phase_error = ph_err;

    % Optional plots
    figure;
    subplot(2,1,1);
    plot(t_common/1000, pr_err);
    xlabel('TOW [s]');
    ylabel('\Delta\rho [m]');
    title(sprintf('Pseudorange error (A - B), PRN %d', prn));
    grid on;

    subplot(2,1,2);
    plot(t_common/1000, ph_err);
    xlabel('TOW [s]');
    ylabel('\Delta\phi [rad]');
    title(sprintf('Carrier phase error (A - B), PRN %d', prn));
    grid on;
end

save('PVT_comparison_AB.mat', 'results');
