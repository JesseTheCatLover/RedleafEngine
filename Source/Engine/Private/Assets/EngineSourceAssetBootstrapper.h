//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include <string>
#include <vector>

class ProjectContext;
class VirtualPathMounter;
class AssetImportSubsystem;

class EngineSourceAssetBootstrapper
{
public:
    static bool Bootstrap(
        AssetImportSubsystem& assetImporter,
        const ProjectContext& context,
        const VirtualPathMounter& pathMounter);

private:
    struct FSourceAssetJob
    {
        std::string sourcePath;
        std::string relativePath;

        std::string destinationVirtualFolder;
        std::string compiledPath;
    };

    static std::string NormalizePhysicalForComparison(std::string path);

    static bool IsPathInsideRoot(const std::string& path, const std::string& root);

    static bool IsIgnorableSourceFile(const std::string& path);

    static bool BuildSourceAssetJob(
        const ProjectContext& context,
        const std::string& sourcePath,
        FSourceAssetJob& outJob);

    static bool RemoveOrphanedCompiledAssets(
        const ProjectContext& context,
        const std::vector<FSourceAssetJob>& jobs);

    static bool ProcessSourceFile(
        AssetImportSubsystem& assetImporter,
        const VirtualPathMounter& pathMounter,
        const FSourceAssetJob& job);
};