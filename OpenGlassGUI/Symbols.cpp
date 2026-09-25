#include "pch.h"
#include "ApplicationPaths.hpp"
#include "PeCodeViewIdentity.hpp"
#include "Symbols.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.Filters.h>
#include <wil/cppwinrt.h>

#include <chrono>
#include <filesystem>
#include <roapi.h>

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "windowsapp.lib")

namespace OpenGlass
{
	namespace
	{

		struct RawDownloadProgress
		{
			double percent{ -1.0 };
			uint64_t totalBytes{ 0 };
			uint64_t downloadedBytes{ 0 };
			uint32_t lastChunkBytes{ 0 };
		};

		struct ModuleDownloadInfo
		{
			std::wstring moduleName;
			std::wstring modulePath;
			std::wstring pdbFileName;
			std::wstring url;
		};

		class CSymbolDownloader
		{
			winrt::Windows::Web::Http::HttpClient m_client{};

		public:
			using Callback = std::function<void(const RawDownloadProgress&)>;

			CSymbolDownloader() = default;

			HRESULT Download(
				const std::stop_token& stopToken,
				const winrt::hstring& url,
				const winrt::hstring& destinationPath,
				const Callback& progressCallback
			) try
			{
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_CANCELLED), stopToken.stop_requested());

				if (progressCallback)
				{
					progressCallback(RawDownloadProgress{});
				}

				const winrt::Windows::Foundation::Uri uri{ url };
				const winrt::Windows::Web::Http::HttpRequestMessage request{ winrt::Windows::Web::Http::HttpMethod::Get(), uri };
				auto responseOperation = m_client.SendRequestAsync(
					request,
					winrt::Windows::Web::Http::HttpCompletionOption::ResponseHeadersRead
				);
				if (responseOperation.wait_for(std::chrono::seconds{ SymbolDownloadTimeoutSeconds }) == winrt::Windows::Foundation::AsyncStatus::Started)
				{
					responseOperation.Cancel();
					THROW_HR(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
				}
				const winrt::Windows::Web::Http::HttpResponseMessage response = responseOperation.get();
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_CANCELLED), stopToken.stop_requested());
				response.EnsureSuccessStatusCode();

				uint64_t totalBytes = 0;
				if (const auto length = response.Content().Headers().ContentLength())
				{
					totalBytes = length.Value();
				}

				std::filesystem::path destination{ destinationPath.c_str() };
				std::error_code removeError;
				std::filesystem::remove(destination, removeError);

				wil::unique_hfile file
				{
					CreateFileW(
						destination.c_str(),
						GENERIC_WRITE,
						0,
						nullptr,
						CREATE_ALWAYS,
						FILE_ATTRIBUTE_NORMAL,
						nullptr
					)
				};
				THROW_LAST_ERROR_IF(!file);

				bool completed = false;
				const auto cleanup = wil::scope_exit([&]
				{
					file.reset();
					if (!completed)
					{
						std::error_code cleanupError;
						std::filesystem::remove(destination, cleanupError);
					}
				});

				auto streamOperation = response.Content().ReadAsInputStreamAsync();
				if (streamOperation.wait_for(std::chrono::seconds{ SymbolDownloadTimeoutSeconds }) == winrt::Windows::Foundation::AsyncStatus::Started)
				{
					streamOperation.Cancel();
					THROW_HR(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
				}
				const auto stream = streamOperation.get();
				winrt::Windows::Storage::Streams::Buffer buffer{ 128 * 1024 };

				uint64_t downloaded = 0;
				for (;;)
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_CANCELLED), stopToken.stop_requested());

					auto readOperation = stream.ReadAsync(
						buffer,
						buffer.Capacity(),
						winrt::Windows::Storage::Streams::InputStreamOptions::Partial
					);
					if (readOperation.wait_for(std::chrono::seconds{ SymbolDownloadTimeoutSeconds }) == winrt::Windows::Foundation::AsyncStatus::Started)
					{
						readOperation.Cancel();
						THROW_HR(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
					}
					const auto read = readOperation.get();
					if (read.Length() == 0)
					{
						break;
					}

					DWORD written = 0;
					THROW_IF_WIN32_BOOL_FALSE(WriteFile(file.get(), read.data(), read.Length(), &written, nullptr));

					downloaded += read.Length();

					double pct = -1.0;
					if (totalBytes > 0)
					{
						pct = (static_cast<double>(downloaded) / static_cast<double>(totalBytes)) * 100.0;
					}

					if (progressCallback)
					{
						progressCallback(RawDownloadProgress{
							pct,
							totalBytes,
							downloaded,
							static_cast<uint32_t>(read.Length())
						});
					}
				}

				completed = true;
				return S_OK;
			}
			catch (...)
			{
				return wil::ResultFromCaughtException();
			}
		};

		std::wstring GetSystemModulePath(PCWSTR moduleName)
		{
			WCHAR systemPath[MAX_PATH]{};
			const UINT length = GetSystemDirectoryW(systemPath, ARRAYSIZE(systemPath));
			if (length == 0 || length >= ARRAYSIZE(systemPath))
			{
				return {};
			}

			return (std::filesystem::path{ systemPath } / moduleName).wstring();
		}


		std::wstring FormatHResult(HRESULT hr)
		{
			return std::format(L"0x{:08X}", static_cast<unsigned long>(hr));
		}

		std::wstring DescribeDownloadFailure(const std::wstring& itemName, HRESULT hr)
		{
			if (hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT))
			{
				return std::format(
					L"等待 {} 秒后下载 {} 超时。",
					SymbolDownloadTimeoutSeconds,
					itemName
				);
			}

			return std::format(L"下载 {} 失败（{}）。", itemName, FormatHResult(hr));
		}

		HRESULT BuildModuleDownloadInfo(const std::wstring& modulePath, PCWSTR moduleName, LPCWSTR symbolServerBase, ModuleDownloadInfo& info)
		{
			info = {};
			info.moduleName = moduleName;
			info.modulePath = modulePath;
			info.url = symbolServerBase;

			PeCodeViewIdentity identity{};
			RETURN_IF_FAILED(ReadFilePeCodeViewIdentity(modulePath.c_str(), identity));
			info.pdbFileName = identity.pdbName;

			WCHAR identifier[48]{};
			swprintf_s(
				identifier,
				L"%08lX%04hX%04hX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%x",
				identity.pdbGuid.Data1,
				identity.pdbGuid.Data2,
				identity.pdbGuid.Data3,
				identity.pdbGuid.Data4[0],
				identity.pdbGuid.Data4[1],
				identity.pdbGuid.Data4[2],
				identity.pdbGuid.Data4[3],
				identity.pdbGuid.Data4[4],
				identity.pdbGuid.Data4[5],
				identity.pdbGuid.Data4[6],
				identity.pdbGuid.Data4[7],
				identity.pdbAge
			);

			info.url += info.pdbFileName;
			info.url += L"/";
			info.url += identifier;
			info.url += L"/";
			info.url += info.pdbFileName;
			return S_OK;
		}

		SymbolDownloadProgress MakeProgress(int percent, bool indeterminate, std::wstring phase, std::wstring detail)
		{
			return SymbolDownloadProgress{
				percent,
				indeterminate,
				std::move(phase),
				std::move(detail)
			};
		}

		void ReportProgress(const SymbolDownloadProgressCallback& progressCallback, SymbolDownloadProgress progress)
		{
			if (progressCallback)
			{
				progressCallback(progress);
			}
		}
	}

	std::wstring GetSymbolCacheDirectory()
	{
		return ApplicationPaths::GetProgramDataSubdirectory(L"symbols").wstring();
	}

	SymbolDownloadOutcome DownloadSymbols(std::wstring symbolDirectory, const std::stop_token& stopToken, const SymbolDownloadProgressCallback& progressCallback)
	{
		SymbolDownloadOutcome outcome{};
		outcome.symbolDirectory = symbolDirectory.empty() ? GetSymbolCacheDirectory() : std::move(symbolDirectory);

		const HRESULT roInitResult = RoInitialize(RO_INIT_MULTITHREADED);
		const auto roScope = wil::scope_exit([roInitResult]
		{
			if (SUCCEEDED(roInitResult))
			{
				RoUninitialize();
			}
		});

		if (FAILED(roInitResult) && roInitResult != RPC_E_CHANGED_MODE)
		{
			outcome.result = SymbolDownloadResult::Failed;
			outcome.hr = roInitResult;
			outcome.summary = std::format(L"初始化 Windows Runtime 失败（{}）。", FormatHResult(roInitResult));
			return outcome;
		}

		std::error_code directoryError;
		std::filesystem::create_directories(outcome.symbolDirectory, directoryError);
		if (directoryError)
		{
			outcome.result = SymbolDownloadResult::Failed;
			outcome.hr = HRESULT_FROM_WIN32(directoryError.value());
			outcome.summary = std::format(L"创建符号目录失败：\n{}", outcome.symbolDirectory);
			return outcome;
		}

		const auto downloader = std::make_unique<CSymbolDownloader>();
		constexpr LPCWSTR symbolServerBase = L"https://msdl.microsoft.com/download/symbols/";
		struct ModuleSpec
		{
			LPCWSTR name;
			int basePercent;
		};
		constexpr ModuleSpec modules[]
		{
			{ L"uDWM.dll", 0 },
			{ L"dwmcore.dll", 50 },
		};

		for (const auto& module : modules)
		{
			if (stopToken.stop_requested())
			{
				outcome.result = SymbolDownloadResult::Cancelled;
				outcome.hr = HRESULT_FROM_WIN32(ERROR_CANCELLED);
				outcome.summary = L"符号下载已取消。";
				return outcome;
			}

			const std::wstring modulePath = GetSystemModulePath(module.name);
			if (modulePath.empty())
			{
				outcome.result = SymbolDownloadResult::Failed;
				outcome.hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
				outcome.summary = std::format(L"在系统目录中找不到 {}。", module.name);
				return outcome;
			}

			ModuleDownloadInfo info{};
			HRESULT hr = BuildModuleDownloadInfo(modulePath, module.name, symbolServerBase, info);
			if (FAILED(hr))
			{
				outcome.result = hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) ? SymbolDownloadResult::Cancelled : SymbolDownloadResult::Failed;
				outcome.hr = hr;
				outcome.summary = std::format(L"为 {} 准备符号下载失败（{}）。", module.name, FormatHResult(hr));
				outcome.details = std::format(L"模块：{}\n路径：{}", module.name, modulePath);
				return outcome;
			}

			ReportProgress(
				progressCallback,
				MakeProgress(
					module.basePercent,
					true,
					L"正在连接 Microsoft 符号服务器...",
					std::format(L"正在准备 {}", info.pdbFileName)
				)
			);

			const auto destinationPath = (std::filesystem::path{ outcome.symbolDirectory } / info.pdbFileName).wstring();
			auto rawProgressCallback = [&](const RawDownloadProgress& progress)
			{
				const bool indeterminate = progress.percent < 0.0;
				const int currentFilePercent = indeterminate
					? -1
					: std::clamp(static_cast<int>(progress.percent), 0, 100);
				const int percent = indeterminate
					? module.basePercent
					: std::clamp(module.basePercent + static_cast<int>(progress.percent / 2.0), 0, 100);

				std::wstring detail = info.pdbFileName;
				if (progress.downloadedBytes > 0)
				{
					detail += std::format(L" - {} 字节", progress.downloadedBytes);
					if (progress.totalBytes > 0)
					{
						detail += std::format(L" / {} 字节", progress.totalBytes);
					}
					if (currentFilePercent >= 0)
					{
						detail += std::format(L" ({}%)", currentFilePercent);
					}
				}
				else
				{
					detail = std::format(L"正在向 Microsoft 符号服务器请求 {}", info.pdbFileName);
				}

				ReportProgress(
					progressCallback,
					MakeProgress(
						percent,
						indeterminate,
						indeterminate ? L"正在连接 Microsoft 符号服务器..." : std::format(L"正在下载 {}", info.pdbFileName),
						detail
					)
				);
			};

			hr = downloader->Download(stopToken, info.url.c_str(), destinationPath.c_str(), rawProgressCallback);
			if (FAILED(hr))
			{
				outcome.result = hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) ? SymbolDownloadResult::Cancelled : SymbolDownloadResult::Failed;
				outcome.hr = hr;
				outcome.summary = outcome.result == SymbolDownloadResult::Cancelled
					? L"符号下载已取消。"
					: DescribeDownloadFailure(info.pdbFileName, hr);
				outcome.details = std::format(
					L"模块：{}\n路径：{}\nPDB：{}\nURL：{}",
					info.moduleName,
					info.modulePath,
					info.pdbFileName,
					info.url
				);
				return outcome;
			}

			ReportProgress(
				progressCallback,
				MakeProgress(
					module.basePercent + 50,
					false,
					std::format(L"{} 下载完成", info.pdbFileName),
					L"本地符号缓存已更新。"
				)
			);
		}

		outcome.result = SymbolDownloadResult::Success;
		outcome.summary = L"所需符号下载成功。";
		return outcome;
	}
}
