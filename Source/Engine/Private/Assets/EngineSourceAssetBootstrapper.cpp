// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "EngineSourceAssetBootstrapper.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AssetImportSubsystem.h"
#include "Assets/FAssetImportRequest.h"
#include "Assets/FAssetImportResult.h"
#include "Core/Project/ProjectContext.h"

#include "Utilities/UFileSystem.h"
#include "Utilities/UPath.h"

std::string EngineSourceAssetBootstrapper::NormalizePhysicalForComparison(std::string path)
{
    path = UPath::NormalizePhysical(path);

    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }

    while (path.size() > 1 && path.back() == '/')
        path.pop_back();

    return path;
}

bool EngineSourceAssetBootstrapper::IsPathInsideRoot(const std::string& path, const std::string& root)
{
    if (path.empty() || root.empty())
        return false;

    const std::string normalizedPath = NormalizePhysicalForComparison(path);
    const std::string normalizedRoot = NormalizePhysicalForComparison(root);

    if (normalizedPath == normalizedRoot)
        return false;

    return UPath::IsSameOrUnder(normalizedRoot, normalizedPath);
}

bool EngineSourceAssetBootstrapper::IsIgnorableSourceFile(const std::string& path)
{
    const std::string filename = UPath::GetFileName(path);

    if (filename.empty())
        return true;

    if (filename == ".DS_Store")
        return true;

    if (filename == "Thumbs.db")
        return true;

    return filename[0] == '.';
}

bool EngineSourceAssetBootstrapper::BuildSourceAssetJob(
    const ProjectContext& context, const std::string& sourcePath, FSourceAssetJob& outJob)
{
    outJob = {};

    const std::string sourceRoot = UPath::Join(context.GetEngineRoot(), "SourceAssets");
    const std::string assetsRoot = UPath::Join(context.GetEngineRoot(), "Assets");

    if (sourcePath.empty())
        return false;

    if (IsIgnorableSourceFile(sourcePath))
        return false;

    if (UPath::GetExtension(sourcePath).empty())
        return false;

    if (!IsPathInsideRoot(sourcePath, sourceRoot))
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Source asset is outside SourceAssets:\n"
            << "  " << sourcePath << '\n';

        return false;
    }

    const std::string normalizedSource = NormalizePhysicalForComparison(sourcePath);
    const std::string normalizedRoot = NormalizePhysicalForComparison(sourceRoot);
    const std::string relative = normalizedSource.substr(normalizedRoot.size() + 1);

    if (relative.empty())
        return false;

    const std::string relativeFolder = UPath::GetParent(relative);
    const std::string filenameNoExt = UPath::GetFileName(relative, false);

    if (filenameNoExt.empty())
        return false;

    std::string destinationVirtualFolder = "/Engine";

    if (!relativeFolder.empty())
        destinationVirtualFolder = UPath::Join("/Engine", relativeFolder);

    std::string compiledPath = assetsRoot;

    if (!relativeFolder.empty())
        compiledPath = UPath::Join(compiledPath, relativeFolder);

    compiledPath = UPath::Join(compiledPath, filenameNoExt + ".jasset");

    outJob.sourcePath = normalizedSource;
    outJob.relativePath = relative;
    outJob.destinationVirtualFolder = destinationVirtualFolder;
    outJob.compiledPath = compiledPath;

    return true;
}

bool EngineSourceAssetBootstrapper::RemoveOrphanedCompiledAssets(
    const ProjectContext& context, const std::vector<FSourceAssetJob>& jobs)
{
    const std::string assetsRoot = UPath::Join(context.GetEngineRoot(), "Assets");

    std::unordered_set<std::string> expectedAssets;
    expectedAssets.reserve(jobs.size());

    for (const FSourceAssetJob& job : jobs)
        expectedAssets.insert(NormalizePhysicalForComparison(job.compiledPath));

    const std::vector<std::string> compiledFiles =
        UFileSystem::ListFiles(assetsRoot, "jasset", true, true);

    bool allGood = true;

    for (const std::string& compiledPath : compiledFiles)
    {
        if (!IsPathInsideRoot(compiledPath, assetsRoot))
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Compiled asset is outside Assets:\n"
                << "  " << compiledPath << '\n';

            allGood = false;
            continue;
        }

        const std::string normalizedPath = NormalizePhysicalForComparison(compiledPath);

        if (expectedAssets.contains(normalizedPath))
            continue;

        if (!UFileSystem::DeleteFile(compiledPath))
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Failed to remove orphaned compiled asset:\n"
                << "  " << compiledPath << '\n';

            allGood = false;
        }
    }

    return allGood;
}

bool EngineSourceAssetBootstrapper::ProcessSourceFile(
    AssetImportSubsystem& assetImporter, const VirtualPathMounter& pathMounter, const FSourceAssetJob& job)
{
    if (!UFileSystem::FileExists(job.sourcePath))
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Source asset disappeared before import:\n"
            << "  " << job.sourcePath << '\n';

        return false;
    }

    const bool compiledAssetExists = UFileSystem::FileExists(job.compiledPath);
    bool shouldImport = !compiledAssetExists;

    if (compiledAssetExists)
    {
        const auto sourceTime = UFileSystem::GetLastWriteTime(job.sourcePath);
        const auto compiledTime = UFileSystem::GetLastWriteTime(job.compiledPath);

        if (!sourceTime || !compiledTime)
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Failed to determine asset timestamps:\n"
                << "  Source:   " << job.sourcePath << '\n'
                << "  Compiled: " << job.compiledPath << '\n';

            return false;
        }

        shouldImport = *sourceTime > *compiledTime;
    }

    if (!shouldImport)
        return true;

    const bool hadExistingAsset = UFileSystem::FileExists(job.compiledPath);
    const std::string backupPath = job.compiledPath + ".bootstrap_backup";

    if (hadExistingAsset)
    {
        if (UFileSystem::FileExists(backupPath) && !UFileSystem::DeleteFile(backupPath))
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Failed to remove stale backup:\n"
                << "  " << backupPath << '\n';

            return false;
        }

        if (!UFileSystem::MoveFile(job.compiledPath, backupPath))
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Failed to stage stale compiled asset:\n"
                << "  Asset:  " << job.compiledPath << '\n'
                << "  Backup: " << backupPath << '\n';

            return false;
        }
    }

    const auto rollback = [&]()
    {
        bool rollbackSuccessful = true;

        if (UFileSystem::FileExists(job.compiledPath) &&
            !UFileSystem::DeleteFile(job.compiledPath))
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Failed to remove failed import output:\n"
                << "  " << job.compiledPath << '\n';

            rollbackSuccessful = false;
        }

        if (hadExistingAsset && UFileSystem::FileExists(backupPath))
        {
            if (!UFileSystem::MoveFile(backupPath, job.compiledPath))
            {
                std::cerr
                    << "[EngineSourceAssetBootstrapper]: "
                    << "Failed to restore previous compiled asset:\n"
                    << "  Backup:      " << backupPath << '\n'
                    << "  Destination: " << job.compiledPath << '\n';

                rollbackSuccessful = false;
            }
        }

        return rollbackSuccessful;
    };

    FAssetImportRequest request;
    request.sourceFilePath = job.sourcePath;
    request.destinationVirtualFolder = job.destinationVirtualFolder;

    FAssetImportResult result;
    bool importSuccess = false;

    try
    {
        importSuccess = assetImporter.Import(request, pathMounter, result);
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Importer threw an exception:\n"
            << "  Source: " << job.sourcePath << '\n'
            << "  Error: " << exception.what() << '\n';

        rollback();
        return false;
    }
    catch (...)
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Importer threw an unknown exception:\n"
            << "  Source: " << job.sourcePath << '\n';

        rollback();
        return false;
    }

    if (!importSuccess)
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Import failed:\n"
            << "  Source: " << job.sourcePath << '\n';

        rollback();
        return false;
    }

    if (!UFileSystem::FileExists(job.compiledPath))
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Importer reported success, but compiled asset was not produced:\n"
            << "  Source:   " << job.sourcePath << '\n'
            << "  Expected: " << job.compiledPath << '\n';

        rollback();
        return false;
    }

    if (hadExistingAsset &&
        UFileSystem::FileExists(backupPath) &&
        !UFileSystem::DeleteFile(backupPath))
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Imported successfully, but failed to remove the previous asset backup:\n"
            << "  " << backupPath << '\n';

        return false;
    }

    return true;
}

bool EngineSourceAssetBootstrapper::Bootstrap(
    AssetImportSubsystem& assetImporter, const ProjectContext& context, const VirtualPathMounter& pathMounter)
{
    const std::string sourceAssetsRoot = UPath::Join(context.GetEngineRoot(), "SourceAssets");
    const std::string engineAssetsRoot = UPath::Join(context.GetEngineRoot(), "Assets");

    if (!UFileSystem::DirectoryExists(sourceAssetsRoot) &&
        !UFileSystem::CreateDirectory(sourceAssetsRoot))
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Could not create SourceAssets directory:\n"
            << "  " << sourceAssetsRoot << '\n';

        return false;
    }

    if (!UFileSystem::DirectoryExists(engineAssetsRoot) &&
        !UFileSystem::CreateDirectory(engineAssetsRoot))
    {
        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Could not create Assets directory:\n"
            << "  " << engineAssetsRoot << '\n';

        return false;
    }

    std::vector<std::string> files = UFileSystem::ListFiles(sourceAssetsRoot, "", true, true);
    std::sort(files.begin(), files.end());

    std::vector<FSourceAssetJob> jobs;
    jobs.reserve(files.size());

    // Build the complete source tree before modifying compiled assets.
    for (const std::string& file : files)
    {
        if (IsIgnorableSourceFile(file))
            continue;

        if (UPath::GetExtension(file).empty())
            continue;

        FSourceAssetJob job;

        if (!BuildSourceAssetJob(context, file, job))
        {
            std::cerr
                << "[EngineSourceAssetBootstrapper]: "
                << "Failed to build source asset job:\n"
                << "  " << file << '\n';

            return false;
        }

        jobs.emplace_back(std::move(job));
    }

    std::unordered_map<std::string, std::string> destinationOwners;
    destinationOwners.reserve(jobs.size());

    for (const FSourceAssetJob& job : jobs)
    {
        const std::string destinationKey = NormalizePhysicalForComparison(job.compiledPath);

        const auto [it, inserted] = destinationOwners.emplace(destinationKey, job.sourcePath);

        if (inserted)
            continue;

        std::cerr
            << "[EngineSourceAssetBootstrapper]: "
            << "Asset output collision detected:\n"
            << "  Output:   " << job.compiledPath << '\n'
            << "  Source A: " << it->second << '\n'
            << "  Source B: " << job.sourcePath << '\n';

        return false;
    }

    if (!RemoveOrphanedCompiledAssets(context, jobs))
        return false;

    for (const FSourceAssetJob& job : jobs)
    {
        if (!ProcessSourceFile(assetImporter, pathMounter, job))
            return false;
    }

    return true;
}