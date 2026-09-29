// Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#pragma once
#include <string>
#include <vector>
#include <filesystem>

/**
 * @class UPath
 * @brief Utility for path manipulation in the engine.
 *
 * UPath is intended to provide lightweight, cross-platform helpers for:
 * - Joining path segments
 * - Lexically normalizing paths
 * - Querying parent/name/extension information
 *
 * Important:
 * - UPath should be treated primarily as a path utility, not as a source of truth
 *   for project or engine roots.
 * - ProjectContext and VirtualPathMounter are now the authoritative owners of
 *   engine/project layout and virtual asset path resolution.
 */
class UPath
{
public:
    // ----------------- Path Manipulation -----------------

    /**
     * @brief Joins multiple path segments into a single lexically normalized path.
     *
     * Concatenates all provided path segments using std::filesystem::path semantics,
     * then performs lexical normalization. This function does not resolve relative paths
     * against any project or engine root.
     *
     * @tparam Args Variadic list of path segment types convertible to std::filesystem::path.
     * @param args Path segments to join (e.g. folder names, filenames).
     * @return Joined and lexically normalized path as a string using forward slashes.
     */
    template<typename... Args>
    static std::string Join(const Args&... args)
    {
        std::filesystem::path p;
        ((p /= std::filesystem::path(args)), ...);
        return NormalizeVirtual(p.string());
    }

    /**
     * @brief Lexically normalizes a virtual engine path.
     *
     * Virtual paths are engine-defined paths such as:
     *
     *     /Project/Textures/Diffuse.jasset
     *     /Engine/Materials/Default.jasset
     *
     * The function:
     *
     * - Converts all separators to forward slashes ('/')
     * - Forces the path to begin with '/'
     * - Collapses repeated slashes
     * - Resolves "." and ".." segments lexically
     * - Removes trailing slashes except for the virtual root ('/')
     *
     * The returned path is always in lexically normalized virtual-path form and should
     * be used for asset references, mount points, registry paths, and other
     * engine-facing path systems.
     *
     * This function must NOT be used for operating-system filesystem paths
     * such as:
     *
     *     C:\Projects\MyGame
     *     /home/user/MyGame
     *
     * Examples:
     *
     *     "Textures\\UI//Button.png"
     *         -> "/Textures/UI/Button.png"
     *
     *     "/Project/Textures/../Materials/Default.jasset"
     *         -> "/Project/Materials/Default.jasset"
     *
     * @param path Virtual path to normalize.
     * @return Canonical virtual path using forward slashes.
     */
    static std::string NormalizeVirtual(std::string_view path);

    /**
     * @brief Lexically normalizes a physical filesystem path.
     *
     * Physical paths are operating-system paths such as:
     *
     *     - C:\Projects\MyGame
     *     - D:\RedleafEngine
     *     - /home/user/MyGame
     *
     * This function performs lexical normalization using std::filesystem::path
     * and returns the result using the running platform's native filesystem
     * representation.
     *
     * The function:
     *
     * - Resolves "." and ".." segments lexically
     * - Reduces redundant separators according to std::filesystem rules
     * - Preserves drive letters, root prefixes, and relative/absolute structure
     * - Uses the running platform's native path separator
     * - Does NOT resolve symlinks or query the actual filesystem
     * - Does NOT convert paths into virtual engine format
     *
     * Platform behavior:
     * - Windows output uses backslashes ('\\')
     * - Other platforms use forward slashes ('/')
     * - String encoding follows std::filesystem::path::string() semantics
     *
     * Note:
     * This function performs syntactic normalization only and does not guarantee
     * full filesystem canonicalization.
     *
     * @param path Physical filesystem path.
     * @return Lexically normalized physical filesystem path using the running
     *         platform's native representation.
     */
    static std::string NormalizePhysical(std::string_view path);

    /**
     * @brief Get the parent directory of a path.
     *
     * This is a pure path operation and does not resolve the path relative to any root.
     *
     * @param path Relative or absolute path.
     * @return Parent path as a lexically normalized string using forward slashes.
     */
    static std::string GetParent(const std::string& path);

    /**
     * @brief Get the filename from a path (optionally including the extension).
     *
     * This is a pure path operation and does not resolve the path relative to any root.
     *
     * @param path Relative or absolute path.
     * @param bIncludeExtension If true, include extension in the result.
     * @return Filename string.
     */
    static std::string GetFileName(const std::string& path, bool bIncludeExtension = true);

    /**
     * @brief Get the file extension (without the dot) from a path.
     *
     * This is a pure path operation and does not resolve the path relative to any root.
     *
     * @param path Relative or absolute path.
     * @return File extension string.
     */
    static std::string GetExtension(const std::string& path);

    /**
     * @brief Remove the file extension from a path.
     *
     * Returns the input path with its final file extension removed. The directory
     * portion of the path is preserved and only the filename extension (the portion
     * after the last '.') is stripped.
     *
     * This is a pure path operation and does not resolve the path relative to any
     * project or engine root.
     *
     * Examples:
     * - "/Project/Textures/icon.png" -> "/Project/Textures/icon"
     * - "file.txt" -> "file"
     *
     * @param path Relative or absolute path.
     * @return Path string with the file extension removed.
     */
    static std::string RemoveExtension(const std::string &path);

    /**
     * @brief Check whether a path is absolute.
     *
     * @param path Relative or absolute path.
     * @return true if the path is absolute, false otherwise.
     */
    static bool IsAbsolute(const std::string& path);

    // ----------------- Name Validation -----------------

    /**
     * @brief Check whether a filesystem/asset name is valid.
     *
     * Validation is platform-safe and intended for engine asset/folder naming.
     *
     * Rules:
     * - Rejects empty names
     * - Rejects path separators
     * - Rejects reserved filesystem characters
     * - Rejects control characters
     * - Rejects trailing spaces/dots
     * - Rejects reserved Windows device names
     *
     * This validates a SINGLE path component only,
     * not an entire path.
     *
     * Examples:
     * - "Textures"      -> valid
     * - "My Folder"     -> valid
     * - "CON"           -> invalid
     * - "File?.png"     -> invalid
     * - "Folder/Sub"    -> invalid
     *
     * @param name Single filename/folder name.
     * @return True if valid.
     */
    static bool IsValidFileSystemName(std::string_view name);

    /**
     * @brief Check whether a filesystem name is reserved.
     *
     * Reserved names include Windows device names such as:
     * CON, PRN, AUX, NUL, COM1..COM9, LPT1..LPT9
     *
     * Comparison is case-insensitive.
     *
     * @param name Single filename/folder name.
     * @return True if reserved.
     */
    static bool IsReservedFileSystemName(std::string_view name);

    /**
     * @brief Sanitize a filesystem name into a safe engine-compatible form.
     *
     * Invalid filesystem characters are removed or replaced,
     * trailing spaces/dots are trimmed,
     * and empty results fall back to a default safe name.
     *
     * This operates on a SINGLE path component only,
     * not an entire path.
     *
     * Examples:
     * - "My:Folder?" -> "MyFolder"
     * - "  Test. "   -> "Test"
     *
     * @param name Input filename/folder name.
     * @return Sanitized filesystem-safe name.
     */
    static std::string SanitizeFileSystemName(std::string_view name);

    /**
     * @brief Check whether a path is identical to or contained within another path.
     *
     * Performs a lexical hierarchy check after normalizing both paths.
     *
     * Returns true when:
     * - path == ancestor
     * - path is located somewhere under ancestor
     *
     * Returns false when:
     * - path is outside ancestor
     * - the match is only a string prefix
     *
     * Examples:
     * - "/Project" + "/Project"                     -> true
     * - "/Project" + "/Project/Textures"            -> true
     * - "/Project" + "/Project/Textures/UI"         -> true
     * - "/Project" + "/ProjectX"                    -> false
     * - "/Project/Textures" + "/Project"            -> false
     *
     * The comparison is path-aware and enforces directory boundaries,
     * preventing false positives caused by simple string prefix matching.
     *
     * Both paths are normalized internally before comparison.
     *
     * @param ancestor The candidate ancestor folder path.
     * @param path The path being tested.
     * @return True if path is the same as or located under ancestor.
     */
    static bool IsSameOrUnder(const std::string& ancestor, const std::string& path);

    /**
     * @brief Determine whether a path is a physical filesystem path.
     *
     * This function is used to distinguish between:
     *
     * - Physical OS paths (filesystem paths handled by the operating system)
     * - Virtual engine paths (engine-defined asset paths starting with '/')
     *
     * Physical paths typically include:
     *
     * - Windows drive letters:
     *     C:\Projects\MyGame
     *     D:\RedleafEngine\Assets
     *
     * - POSIX absolute paths:
     *     /home/user/MyGame
     *     /usr/local/bin
     *
     * - Relative filesystem paths:
     *     ./Assets/Textures
     *     ../Config/settings.json
     *
     * Virtual paths are NOT considered physical:
     *
     *     /Project/Textures/Diffuse.jasset
     *     /Engine/Materials/Default.jasset
     *
     * ### Detection rules:
     * The function classifies a path as physical if:
     *
     * - It contains a Windows drive letter (e.g. "C:", "D:")
     * - OR it is a POSIX-style filesystem path NOT following engine virtual conventions
     * - OR it does NOT start with a recognized engine virtual root (e.g. "/Project", "/Engine")
     *
     * It returns false for engine virtual paths, which are expected to:
     * - Always use forward slashes
     * - Always begin with '/'
     * - Represent mounted engine namespaces rather than OS locations
     *
     * ### Examples:
     *
     *     "C:\\Projects\\Game"
     *         -> true
     *
     *     "/home/user/Game"
     *         -> true
     *
     *     "./Assets/Textures"
     *         -> true
     *
     *     "/Project/Textures/UI"
     *         -> false
     *
     *     "/Engine/Core/Default"
     *         -> false
     *
     * @param p Input path string to evaluate.
     * @return True if the path is a physical filesystem path, false if it is a virtual engine path.
     */
    static bool IsPhysicalPath(std::string_view p);
};