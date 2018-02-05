#include "project/SourceGroupJavaMaven.h"

#include "component/view/DialogView.h"
#include "settings/ApplicationSettings.h"
#include "utility/file/FileSystem.h"
#include "utility/messaging/type/MessageStatus.h"
#include "utility/ScopedFunctor.h"
#include "utility/utility.h"
#include "utility/utilityMaven.h"
#include "Application.h"

SourceGroupJavaMaven::SourceGroupJavaMaven(std::shared_ptr<SourceGroupSettingsJavaMaven> settings)
	: m_settings(settings)
{
}

SourceGroupJavaMaven::~SourceGroupJavaMaven()
{
}

SourceGroupType SourceGroupJavaMaven::getType() const
{
	return SOURCE_GROUP_JAVA_MAVEN;
}

bool SourceGroupJavaMaven::prepareIndexing()
{
	if (!SourceGroupJava::prepareIndexing())
	{
		return false;
	}

	if (!prepareMavenData())
	{
		return false;
	}

	return true;
}

std::vector<FilePath> SourceGroupJavaMaven::doGetClassPath() const
{
	std::vector<FilePath> classPath = SourceGroupJava::doGetClassPath();

	if (m_settings && m_settings->getMavenDependenciesDirectoryExpandedAndAbsolute().exists())
	{
		std::vector<FilePath> mavenJarPaths = FileSystem::getFilePathsFromDirectory(
			m_settings->getMavenDependenciesDirectoryExpandedAndAbsolute(),
			{ L".jar" }
		);

		for (const FilePath& mavenJarPath : mavenJarPaths)
		{
			LOG_INFO("Adding jar to classpath: " + mavenJarPath.str());
		}

		utility::append(classPath, mavenJarPaths);
	}

	return classPath;
}

std::shared_ptr<SourceGroupSettingsJava> SourceGroupJavaMaven::getSourceGroupSettingsJava()
{
	return m_settings;
}

std::shared_ptr<const SourceGroupSettingsJava> SourceGroupJavaMaven::getSourceGroupSettingsJava() const
{
	return m_settings;
}

std::vector<FilePath> SourceGroupJavaMaven::getAllSourcePaths() const
{
	std::vector<FilePath> sourcePaths;
	if (m_settings && m_settings->getMavenProjectFilePathExpandedAndAbsolute().exists())
	{
		std::shared_ptr<DialogView> dialogView = Application::getInstance()->getDialogView();
		dialogView->showUnknownProgressDialog("Preparing Project", "Maven\nFetching Source Directories");

		const FilePath mavenPath(ApplicationSettings::getInstance()->getMavenPath());
		const FilePath projectRootPath = m_settings->getMavenProjectFilePathExpandedAndAbsolute().getParentDirectory();
		sourcePaths = utility::mavenGetAllDirectoriesFromEffectivePom(mavenPath, projectRootPath, m_settings->getShouldIndexMavenTests());

		dialogView->hideUnknownProgressDialog();
	}
	return sourcePaths;
}

bool SourceGroupJavaMaven::prepareMavenData()
{
	if (m_settings && m_settings->getMavenProjectFilePathExpandedAndAbsolute().exists())
	{
		const FilePath mavenPath = ApplicationSettings::getInstance()->getMavenPath();
		const FilePath projectRootPath = m_settings->getMavenProjectFilePathExpandedAndAbsolute().getParentDirectory();

		std::shared_ptr<DialogView> dialogView = Application::getInstance()->getDialogView();
		dialogView->showUnknownProgressDialog("Preparing Project", "Maven\nGenerating Source Files");

		ScopedFunctor dialogHider([&dialogView](){
			dialogView->hideUnknownProgressDialog();
		});

		bool success = utility::mavenGenerateSources(mavenPath, projectRootPath);

		if (!success)
		{
			const std::wstring dialogMessage =
				L"Sourcetrail was unable to locate Maven on this machine.\n"
				"Please make sure to provide the correct Maven Path in the preferences.";

			MessageStatus(dialogMessage, true, false).dispatch();
			Application::getInstance()->handleDialog(dialogMessage);
		}

		if (success)
		{
			dialogView->showUnknownProgressDialog("Preparing Project", "Maven\nExporting Dependencies");

			success = utility::mavenCopyDependencies(
				mavenPath, projectRootPath, m_settings->getMavenDependenciesDirectoryExpandedAndAbsolute()
			);
		}

		return success;
	}

	return true;
}
