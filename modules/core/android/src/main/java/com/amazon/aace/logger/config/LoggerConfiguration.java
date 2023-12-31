/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.amazon.aace.logger.config;

import com.amazon.aace.core.config.EngineConfiguration;
import com.amazon.aace.logger.Logger;

/**
 *
 * @code{.json}
 * {
 *   "aace.logger":
 *   {
 *      "sinks": [<Sink>],
 *      "rules": [{"sink": "<SINK_ID>", "rule": <Rule>}]
 *   }
 * }
 *
 * <Sink>: {
 *   "id": "<SINK_ID>"
 *   "type": "<SINK_TYPE>",
 *   "config": {
 *     <CONFIG_DATA>
 *   },
 *   "rules": [<RuleConfiguration>]
 * }
 *
 * <Rule>: {
 *   "level": "<LOG_LEVEL>",
 *   "source": "<SOURCE_FILTER>",
 *   "tag": "<TAG_FILTER>",
 *   "message": "<MESSAGE_FILTER>"
 * }
 * @endcode
 */
public class LoggerConfiguration {
    /**
     * Factory method used to programmatically generate logger configuration data for a console sink.
     * The data generated by this method is equivalent to providing the following JSON values
     * in a configuration file:
     *
     * @code    {.json}
     * {
     *   "aace.logger":
     *   {
     *     "sinks": [{
     *       "id": "<SINK_ID>",
     *       "type": "aace.logger.sink.console",
     *       "rules": [{
     *         "level": <LOG_LEVEL>
     *       }]
     *     }
     *   }
     * }
     * @endcode
     *
     * @param  id The id of sink object
     *
     * @param  level The log level to be used to filter logs to this sink
     */
    public static EngineConfiguration createConsoleSinkConfig(final String id, final Logger.Level level) {
        return new EngineConfiguration() {
            @Override
            protected long createNativeRef() {
                return createConsoleSinkConfigBinder(id, level);
            }
        };
    }

    // Native Engine JNI methods
    static private native long createConsoleSinkConfigBinder(String id, Logger.Level level);

    /**
     * Factory method used to programmatically generate logger configuration data for a syslog sink.
     * The data generated by this method is equivalent to providing the following JSON values
     * in a configuration file:
     *
     * @code    {.json}
     * {
     *   "aace.logger":
     *   {
     *     "sinks": [{
     *       "id": "<SINK_ID>",
     *       "type": "aace.logger.sink.syslog",
     *       "rules": [{
     *         "level": <LOG_LEVEL>
     *       }]
     *     }
     *   }
     * }
     * @endcode
     *
     * @param  id The id of sink object
     *
     * @param  level The log level to be used to filter logs to this sink
     */
    public static EngineConfiguration createSyslogSinkConfig(final String id, final Logger.Level level) {
        return new EngineConfiguration() {
            @Override
            protected long createNativeRef() {
                return createSyslogSinkConfigBinder(id, level);
            }
        };
    }

    // Native Engine JNI methods
    static private native long createSyslogSinkConfigBinder(String id, Logger.Level level);

    /**
     * Factory method used to programmatically generate logger configuration data for a file sink.
     * The data generated by this method is equivalent to providing the following JSON values
     * in a configuration file:
     *
     * @code    {.json}
     * {
     *   "aace.logger":
     *   {
     *     "sinks": [{
     *       "id": "<SINK_ID>",
     *       "type": "aace.logger.sink.file",
     *       "config": {
     *         "path": "<PATH>",
     *         "prefix": "<PREFIX>",
     *         "maxSize": <MAX_SIZE>,
     *         "maxFiles": <MAX_FILES>,
     *         "append": <APPEND>
     *       }
     *       "rules": [{
     *         "level": <LOG_LEVEL>
     *       }]
     *     }
     *   }
     * }
     * @endcode
     *
     * @param  id The id of sink object
     *
     * @param  level The log level to be used to filter logs to this sink
     *
     * @param  path The parent path where the log files will be written (must exist)
     *
     * @param  prefix The prefix name given to the log file
     *
     * @param  maxSize The maximum log file size in bytes
     *
     * @param  maxFiles The maximum number of log files to rotate
     *
     * @param  append @c true If the logs should be appended to the existing file, @c false if the file should be
     *         overwritten
     */
    public static EngineConfiguration createFileSinkConfig(final String id, final Logger.Level level, final String path,
            final String prefix, final int maxSize, final int maxFiles, final boolean append) {
        return new EngineConfiguration() {
            @Override
            protected long createNativeRef() {
                return createFileSinkConfigBinder(id, level, path, prefix, maxSize, maxFiles, append);
            }
        };
    }

    // Native Engine JNI methods
    static private native long createFileSinkConfigBinder(
            String id, Logger.Level level, String path, String prefix, int maxSize, int maxFiles, boolean append);

    /**
     * Factory method used to programmatically generate logger configuration data for a file sink.
     * The data generated by this method is equivalent to providing the following JSON values
     * in a configuration file:
     *
     * @code    {.json}
     * {
     *   "aace.logger":
     *   {
     *     "sinks": [{
     *       "id": "<SINK_ID>",
     *       "type": "aace.logger.sink.file",
     *       "config": {
     *         "path": "<PATH>",
     *         "prefix": "<PREFIX>",
     *         "maxSize": <MAX_SIZE>,
     *         "maxFiles": <MAX_FILES>,
     *         "append": <APPEND>
     *       }
     *       "rules": [{
     *         "level": <LOG_LEVEL>
     *       }]
     *     }
     *   }
     * }
     * @endcode
     *
     * @param  id The id of sink object
     *
     * @param  level The log level to be used to filter logs to this sink
     *
     * @param  path The parent path where the log files will be written (must exist)
     *
     * @param  prefix The prefix name given to the log file
     *
     * @param  maxSize The maximum log file size in bytes
     *
     * @param  maxFiles The maximum number of log files to rotate
     *
     * @param  append @c true If the logs should be appended to the existing file, @c false if the file should be
     *         overwritten
     */
    public static EngineConfiguration createFileSinkConfig(String id, Logger.Level level, String path) {
        return createFileSinkConfig(id, level, path, "aace", 5242880, 3, true);
    }

    /**
     * Factory method used to programmatically generate configuration data for a logger rule.
     * The data generated by this method is equivalent to providing the following JSON values
     * in a configuration file:
     *
     * @code    {.json}
     * {
     *   "aace.logger":
     *   {
     *     "rules": [{
     *       "sink": "<SINK_ID>",
     *       "rule": {
     *         "level": <LOG_LEVEL>,
     *         "source": "<SOURCE_FILTER>",
     *         "tag": "<TAG_FILTER>",
     *         "message": "<MESSAGE_FILTER>"
     *       }
     *     }
     *   }
     * }
     * @endcode
     *
     * @param  sink The id of sink object to which this rule is applied
     *
     * @param  level The log level to be used as a filter for this rule
     *
     * @param  sourceFilter The source regex to be used as a filter for this rule
     *
     * @param  tagFilter The tag regex to be used as a filter for this rule
     *
     * @param  messageFilter The message regex to be used as a filter for this rule
     */
    public static EngineConfiguration createLoggerRuleConfig(final String sink, final Logger.Level level,
            final String sourceFilter, final String tagFilter, final String messageFilter) {
        return new EngineConfiguration() {
            @Override
            protected long createNativeRef() {
                return createLoggerRuleConfigBinder(sink, level, sourceFilter, tagFilter, messageFilter);
            }
        };
    }

    // Native Engine JNI methods
    static private native long createLoggerRuleConfigBinder(
            String sink, Logger.Level level, String sourceFilter, String tagFilter, String messageFilter);

    /**
     * Factory method used to programmatically generate configuration data for a logger rule.
     * The data generated by this method is equivalent to providing the following JSON values
     * in a configuration file:
     *
     * @code    {.json}
     * {
     *   "aace.logger":
     *   {
     *     "rules": [{
     *       "sink": "<SINK_ID>",
     *       "rule": {
     *         "level": <LOG_LEVEL>,
     *         "source": "<SOURCE_FILTER>",
     *         "tag": "<TAG_FILTER>",
     *         "message": "<MESSAGE_FILTER>"
     *       }
     *     }
     *   }
     * }
     * @endcode
     *
     * @param  sink The id of sink object to which this rule is applied
     *
     * @param  level The log level to be used as a filter for this rule
     *
     * @param  sourceFilter The source regex to be used as a filter for this rule
     *
     * @param  tagFilter The tag regex to be used as a filter for this rule
     *
     * @param  messageFilter The message regex to be used as a filter for this rule
     */
    public static EngineConfiguration createLoggerRuleConfig(String sink, Logger.Level level) {
        return createLoggerRuleConfig(sink, level, "", "", "");
    }
}
