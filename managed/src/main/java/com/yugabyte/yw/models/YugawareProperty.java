// Copyright (c) YugaByte, Inc.

package com.yugabyte.yw.models;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.avaje.ebean.Model;
import com.avaje.ebean.annotation.EnumValue;

import play.data.validation.Constraints;

/**
 * Table that has all the configuration data for Yugaware, such as the latest stable release version
 * of YB to deploy, etc. Each entry is a name-value pair.
 */
@Entity
public class YugawareProperty extends Model {
  public static final Logger LOG = LoggerFactory.getLogger(YugawareProperty.class);

  // The name of the property.
  @Id
  private String name;

  // The types of entries in this table.
  private enum PropertyEntryType {
    // Externally specified configuration properties. E.g. supported machine types, etc.
    @EnumValue("Config")
    Config,

    // System maintained properties. E.g. system maintained stats on instance provision time.
    @EnumValue("System")
    System,
  }
  @Constraints.Required
  @Column(nullable = false)
  private PropertyEntryType type;

  // The property config.
  @Constraints.Required
  @Column(columnDefinition = "TEXT")
  private String value;

  // The property description.
  @Column(columnDefinition = "TEXT")
  private String description;

  private static final Find<String, YugawareProperty> find = new Find<String, YugawareProperty>(){};

  private YugawareProperty(String name, PropertyEntryType type, String value, String description) {
    this.name = name;
    this.type = type;
    this.value = value;
    this.description = description;
  }

  /**
   * These are externally specified configuration properties. These are not updated by the system at
   * runtime. They can be modified while the system is running, and will take effect from the next
   * task that runs.
   *
   * @param name is the property name
   * @param value is the property value
   * @param description is a description of the property
   */
  public static void addConfigProperty(String name,
                                       String value,
                                       String description) {
    if (find.byId(name) != null) {
      LOG.warn("Property " + name + " already present, skipping.");
      return;
    }
    YugawareProperty entry =
        new YugawareProperty(name, PropertyEntryType.Config, value, description);
    entry.save();
  }
}
