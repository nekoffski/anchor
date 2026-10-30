#include <sstream>

#include "Application.hh"
#include "TestSupport.hh"
#include "core/Error.hh"
#include "proto/AckResponse.hh"
#include "proto/DiagnosticRequest.hh"
#include "proto/GetBackupsRequest.hh"
#include "proto/GetBackupsResponse.hh"
#include "proto/ReloadPlansRequest.hh"
#include "proto/TriggerBackupRequest.hh"

namespace anchor::test {
using namespace kc;
using namespace testing;

class MockClient : public cli::Client {
   public:
    MOCK_METHOD(
        Result<std::unique_ptr<proto::Message>>, request,
        (u16, const proto::Message&), (override)
    );
};

class CliTest : public Test {
   protected:
    StrictMock<MockClient> client;
    std::ostringstream output;
    std::ostringstream errors;

    int run(std::initializer_list<const char*> arguments) {
        std::vector<const char*> argv{arguments};
        cli::Application application{client, output, errors};
        return application.run(static_cast<int>(argv.size()), argv.data());
    }
};

TEST_F(CliTest, ListDisplaysSortedBackupsWithActiveAndTerminalRunDetails) {
    auto idle = Backup{.plan = plan(), .enabled = false};
    idle.plan.name = "a-idle";
    auto running = Backup{
        .plan = plan(),
        .run = BackupRunStatus{
            .state = BackupRunState::uploading, .uuid = "active-id"
        }
    };
    auto failed = Backup{
        .plan = plan(),
        .run = BackupRunStatus{
            .state = BackupRunState::failed,
            .error = Error{kc::ErrorCode::fileSystemError, "disk full"},
            .uuid = "failed-id"
        }
    };
    failed.plan.name = "z-failed";
    const std::vector<Backup> backups{failed, running, idle};
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [&](
                u16, const proto::Message& request
            ) -> Result<std::unique_ptr<proto::Message>> {
                EXPECT_NE(request.as<proto::GetBackupsRequest>(), nullptr);
                // Exercise the same serialization/decoding path used by the
                // client.
                return proto::Message::create(
                    proto::GetBackupsResponse{backups}.serialize()
                );
            }
        ));
    EXPECT_EQ(run({"anchor", "backup", "list"}), 0);
    EXPECT_EQ(
        output.str(),
        "Backup: a-idle\n  Enabled: no\n  State: idle\n"
        "Backup: daily\n  Enabled: yes\n  State: uploading\n  Run ID: "
        "active-id\n"
        "Backup: z-failed\n  Enabled: yes\n  State: failed\n  Run ID: "
        "failed-id\n  Error: disk full\n"
    );
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(CliTest, EmptyListHasExplicitOutput) {
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [](
                u16, const proto::Message&
            ) -> Result<std::unique_ptr<proto::Message>> {
                return std::make_unique<proto::GetBackupsResponse>();
            }
        ));
    EXPECT_EQ(run({"anchor", "backup", "list"}), 0);
    EXPECT_EQ(output.str(), "No backups found.\n");
}

TEST_F(
    CliTest, TriggerAcceptsPositionalNameAndLegacyOptionAndGlobalPortAtAnyLevel
) {
    for (const auto& arguments :
         {std::vector<const char*>{
              "anchor", "--port", "1234", "backup", "trigger", "daily"
          },
          std::vector<const char*>{
              "anchor", "backup", "--port", "1234", "trigger", "--name", "daily"
          },
          std::vector<const char*>{
              "anchor", "backup", "trigger", "daily", "--port", "1234"
          }}) {
        EXPECT_CALL(client, request(1234, _))
            .WillOnce(Invoke(
                [](
                    u16, const proto::Message& request
                ) -> Result<std::unique_ptr<proto::Message>> {
                    const auto* trigger =
                        request.as<proto::TriggerBackupRequest>();
                    EXPECT_NE(trigger, nullptr);
                    if (trigger) {
                        EXPECT_EQ(trigger->backupName(), "daily");
                    }
                    return std::make_unique<proto::AckResponse>();
                }
            ));
        cli::Application application{client, output, errors};
        EXPECT_EQ(
            application.run(
                static_cast<int>(arguments.size()), arguments.data()
            ),
            0
        );
    }
    EXPECT_THAT(output.str(), HasSubstr("Backup 'daily' accepted."));
    EXPECT_THAT(output.str(), Not(HasSubstr("completed")));
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(CliTest, ReloadAndDiagnosticsUseAcknowledgementHandling) {
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [](
                u16, const proto::Message& request
            ) -> Result<std::unique_ptr<proto::Message>> {
                EXPECT_NE(request.as<proto::ReloadPlansRequest>(), nullptr);
                return std::make_unique<proto::AckResponse>();
            }
        ));
    EXPECT_EQ(run({"anchor", "backup", "reload"}), 0);
    EXPECT_EQ(output.str(), "Backup plans reloaded.\n");
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [](
                u16, const proto::Message& request
            ) -> Result<std::unique_ptr<proto::Message>> {
                const auto* diagnostics =
                    request.as<proto::DiagnosticRequest>();
                EXPECT_NE(diagnostics, nullptr);
                if (diagnostics) {
                    EXPECT_EQ(diagnostics->details(), "test details");
                }
                return std::make_unique<proto::AckResponse>();
            }
        ));
    EXPECT_EQ(run({"anchor", "diagnostics", "--details", "test details"}), 0);
    EXPECT_THAT(output.str(), HasSubstr("Diagnostics accepted by daemon."));
}

TEST_F(CliTest, TransportAndDaemonErrorsHaveNonzeroExitAndNoSuccessOutput) {
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Return(
            Error::unexpected(
                kc::ErrorCode::connectionFailure, "connection refused"
            )
        ));
    EXPECT_EQ(run({"anchor", "backup", "list"}), 1);
    EXPECT_EQ(errors.str(), "anchor: connection refused\n");
    EXPECT_TRUE(output.str().empty());
    errors.str("");
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [](
                u16, const proto::Message&
            ) -> Result<std::unique_ptr<proto::Message>> {
                return std::make_unique<proto::ErrorResponse>(
                    ErrorCode::alreadyRunning, "Backup is already running"
                );
            }
        ));
    EXPECT_EQ(run({"anchor", "backup", "trigger", "daily"}), 1);
    EXPECT_EQ(errors.str(), "anchor: Backup is already running\n");
    EXPECT_TRUE(output.str().empty());
}

TEST_F(CliTest, WrongResponseTypesFailInsteadOfCrashingOrReportingSuccess) {
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [](
                u16, const proto::Message&
            ) -> Result<std::unique_ptr<proto::Message>> {
                return std::make_unique<proto::AckResponse>();
            }
        ));
    EXPECT_EQ(run({"anchor", "backup", "list"}), 1);
    EXPECT_THAT(errors.str(), HasSubstr("Unexpected response 'AckResponse'"));
    EXPECT_TRUE(output.str().empty());
    errors.str("");
    EXPECT_CALL(client, request(_, _))
        .WillOnce(Invoke(
            [](
                u16, const proto::Message&
            ) -> Result<std::unique_ptr<proto::Message>> {
                return std::make_unique<proto::GetBackupsResponse>();
            }
        ));
    EXPECT_EQ(run({"anchor", "backup", "reload"}), 1);
    EXPECT_THAT(
        errors.str(), HasSubstr("Unexpected response 'GetBackupsResponse'")
    );
    EXPECT_TRUE(output.str().empty());
}

TEST_F(CliTest, InvalidCommandsAndArgumentsNeverContactDaemon) {
    for (const auto& arguments :
         {std::vector<const char*>{"anchor"},
          std::vector<const char*>{"anchor", "backup"},
          std::vector<const char*>{"anchor", "backup", "trigger"},
          std::vector<const char*>{"anchor", "backup", "trigger", ""},
          std::vector<const char*>{"anchor", "backup", "list", "reload"},
          std::vector<const char*>{"anchor", "backup", "activate", "daily"},
          std::vector<const char*>{"anchor", "sync"},
          std::vector<const char*>{"anchor", "backup", "list", "--port", "0"},
          std::vector<const char*>{
              "anchor", "backup", "list", "--port", "65536"
          }}) {
        cli::Application application{client, output, errors};
        EXPECT_NE(
            application.run(
                static_cast<int>(arguments.size()), arguments.data()
            ),
            0
        );
    }
    EXPECT_TRUE(output.str().empty());
}

TEST_F(CliTest, HelpListsOnlyImplementedCommandsWithoutContactingDaemon) {
    EXPECT_EQ(run({"anchor", "backup", "--help"}), 0);
    EXPECT_THAT(output.str(), HasSubstr("list"));
    EXPECT_THAT(output.str(), HasSubstr("trigger"));
    EXPECT_THAT(output.str(), HasSubstr("reload"));
    EXPECT_THAT(output.str(), Not(HasSubstr("mock")));
    EXPECT_THAT(output.str(), Not(HasSubstr("activate")));
    EXPECT_TRUE(errors.str().empty());
}

}  // namespace anchor::test
