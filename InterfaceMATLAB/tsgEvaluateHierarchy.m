function [vals] = tsgEvaluateHierarchy(lGrid, mX)
%
% [vals] = tsgEvaluateHierarchy(lGrid, mX)
%
% WARNING: this is an experimental feature
%
% it gives the weights for interpolation (or approximation)
%
% INPUT:
%
% lGrid: a grid list created by tsgMakeXXX(...) command
%
% mX: an array of size [num_x, dimensions]
%     specifies the points where the basis funcitons should be evaluated
%     Note: do not confuse points here with the nodes of the grid
%           here points are user specified points to evaluate the
%           hierarchical basis functions
%
% OUTPUT:
%
% vals: if lGrid is Global or Sequence
%          vals is an array of size [num_x, number_of_points]
%       if lGrid is LocalPolynomial or Wavelet
%          vals is a sparse matrix of size [num_x, number_of_points]
%
%       in both cases, vals returns
%       the values associated with the hierarchical basis funcitons
%

[sFiles, sTasGrid] = tsgGetPaths();
[sFileG, sFileX, sFileV, sFileO, sFileW, sFileC] = tsgMakeFilenames(lGrid.sName);

if (strcmp(lGrid.sType, 'global') || strcmp(lGrid.sType, 'sequence'))
    sCommand = [sTasGrid,' -evalhierarchyd'];
else
    sCommand = [sTasGrid,' -evalhierarchys'];
end

sCommand = [sCommand, ' -gridfile ', sFileG];

tsgWriteMatrix(sFileX, mX);

sCommand = [sCommand, ' -xf ', sFileX];
lClean.sFileX = 1;

sCommand = [sCommand, ' -of ', sFileO];
lClean.sFileO = 1;

[status, cmdout] = system(sCommand);

if (size(findstr('ERROR', cmdout)) ~= [0, 0])
    disp(cmdout);
    error('The tasgrid execurable returned an error, see above');
    return;
else
    if (~isempty(cmdout))
        fprintf(1,['Warning: Command had non-empty output:\n']);
        disp(cmdout);
    end
    if (strcmp(lGrid.sType, 'global') || strcmp(lGrid.sType, 'sequence'))
        [vals] = tsgReadMatrix(sFileO);
    else
        fid = fopen(sFileO);
        TSG = fread(fid, [1, 3], '*char');
        if (TSG == 'TSG')
            D = fread(fid, [1, 3], '*int');
            Rows = D(1);
            Cols = D(2);
            NNZ = D(3);
            pntr = fread(fid, [Rows+1], '*int')';
            indx = fread(fid, [NNZ], '*int')';
            vals = fread(fid, [NNZ], '*double')';
            rindx = ones(NNZ, 1);
            for i = 1:Rows
                rindx((pntr(i)+1):(pntr(i+1))) = i;
            end
            %[Rows, Cols, NNZ]
            vals = sparse(rindx, indx + 1, vals, Rows, Cols, NNZ);
        else
            frewind(fid);
        end
        fclose(fid);
    end
end

tsgCleanTempFiles(lGrid, lClean);

end
